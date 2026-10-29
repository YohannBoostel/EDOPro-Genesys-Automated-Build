#include "genesys_updater.h"
#include "curl.h"

#include <atomic>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

namespace ygo::genesys {
namespace {

std::atomic<unsigned long> update_counter{0};
std::mutex log_mutex;

void Log(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::ofstream log("./config/genesys_updater.log", std::ios::app);
    if(log)
        log << message << '\n';
}

struct UpdateWorker {
    std::thread thread;

    ~UpdateWorker() {
        if(thread.joinable())
            thread.join();
    }
};

size_t WriteData(void* data, size_t size, size_t count, void* user_data) {
    const size_t bytes = size * count;
    auto* output = static_cast<std::ofstream*>(user_data);
    output->write(static_cast<const char*>(data),
                  static_cast<std::streamsize>(bytes));
    return *output ? bytes : 0;
}

bool DownloadCsv(const std::string& url, const std::string& destination) {
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if(!output) {
        Log("ERRO: nao foi possivel criar o arquivo temporario.");
        return false;
    }

    CURL* curl = curl_easy_init();
    if(!curl) {
        Log("ERRO: curl_easy_init falhou.");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "EDOPro-Genesys-CSV-Updater");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "./cacert.pem");

    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    const std::string error_message = curl_easy_strerror(result);
    curl_easy_cleanup(curl);
    output.close();

    if(result != CURLE_OK || status < 200 || status >= 300 || output.fail()) {
        Log("ERRO no download: " + error_message +
            " | HTTP " + std::to_string(status));
        return false;
    }

    Log("Download concluido. HTTP " + std::to_string(status));
    return true;
}

bool ReplaceFile(const std::string& downloaded, const std::string& local) {
    const std::string backup = local + ".genesys-backup";
    std::remove(backup.c_str());

    const bool had_original = std::ifstream(local, std::ios::binary).good();
    if(had_original && std::rename(local.c_str(), backup.c_str()) != 0) {
        Log("ERRO ao renomear CSV local para backup.");
        return false;
    }

    if(std::rename(downloaded.c_str(), local.c_str()) != 0) {
        Log("ERRO ao instalar CSV baixado.");
        if(had_original && std::rename(backup.c_str(), local.c_str()) != 0)
            Log("ERRO ao restaurar backup.");
        return false;
    }

    std::remove(backup.c_str());
    Log("CSV remoto instalado no caminho local.");
    return true;
}

} // namespace

void StartCsvUpdate(Rules& rules,
                    const std::filesystem::path& local_csv,
                    const std::string& remote_url) {
    static std::once_flag start_flag;
    static UpdateWorker worker;

    const std::string local_path = local_csv.string();

    std::call_once(start_flag, [&rules, local_path, remote_url] {
        Log("Atualizador iniciado. URL: " + remote_url);

        worker.thread = std::thread([&rules, local_path, remote_url] {
            const auto suffix =
                std::to_string(update_counter.fetch_add(1));

            const std::string temp = local_path + ".download-" + suffix;

            if(!DownloadCsv(remote_url, temp)) {
                std::remove(temp.c_str());
                return;
            }

            Rules candidate;
            std::string error;

            if(!candidate.LoadCsv(temp, &error)) {
                Log("ERRO ao validar CSV remoto: " + error);
                std::remove(temp.c_str());
                return;
            }

            Log("CSV remoto validado. Entradas: " +
                std::to_string(candidate.PointEntryCount()));

            if(!ReplaceFile(temp, local_path))
                return;

            if(!rules.LoadCsv(local_path, &error))
                Log("ERRO ao recarregar regras: " + error);
            else
                Log("Regras Genesys atualizadas com sucesso.");
        });
    });
}

} // namespace ygo::genesys
