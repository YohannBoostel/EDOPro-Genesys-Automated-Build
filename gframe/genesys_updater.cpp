
#include "genesys_updater.h"

#include "curl.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace ygo::genesys {
namespace {

std::atomic<unsigned long> update_counter{0};
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

bool DownloadCsv(const std::string& url,
                 const std::filesystem::path& destination) {

    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if(!output)
        return false;

    CURL* curl = curl_easy_init();
    if(!curl)
        return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "EDOPro-Genesys-CSV-Updater");

    const CURLcode result = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    output.close();
    return result == CURLE_OK && status >= 200 && status < 300
           && !output.fail();
}

bool ReplaceFile(const std::filesystem::path& downloaded,
                 const std::filesystem::path& local) {
    std::error_code ec;
    const auto backup = local.string() + ".genesys-backup";

    std::filesystem::remove(backup, ec);
    ec.clear();

    const bool had_original = std::filesystem::exists(local, ec) && !ec;

    if(had_original) {
        std::filesystem::rename(local, backup, ec);
        if(ec)
            return false;
    }

    std::filesystem::rename(downloaded, local, ec);

    if(ec) {
        if(had_original) {
            std::error_code restore_error;
            std::filesystem::rename(backup, local, restore_error);
        }
        return false;
    }

    std::filesystem::remove(backup, ec);
    return true;
}

} // namespace



void StartCsvUpdate(Rules& rules,
                    const std::filesystem::path& local_csv,
                    const std::string& remote_url) {
    static std::once_flag start_flag;
    static UpdateWorker worker;

    std::call_once(start_flag, [&rules, local_csv, remote_url] {
        worker.thread = std::thread([&rules, local_csv, remote_url] {
            std::error_code ec;
            const auto parent = local_csv.parent_path();

            if(!parent.empty())
                std::filesystem::create_directories(parent, ec);

            if(ec)
                return;

            const auto suffix =
                std::to_string(update_counter.fetch_add(1));

            const std::filesystem::path temp =
                local_csv.string() + ".download-" + suffix;

            if(!DownloadCsv(remote_url, temp)) {
                std::filesystem::remove(temp, ec);
                return;
            }

            Rules candidate;
            std::string error;

            if(!candidate.LoadCsv(temp, &error)) {
                std::filesystem::remove(temp, ec);
                return;
            }

            if(!ReplaceFile(temp, local_csv))
                return;

            rules.LoadCsv(local_csv, &error);
        });
    });
}

} // namespace ygo::genesys
