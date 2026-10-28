
import csv
import os
import sys
import urllib.request
from html.parser import HTMLParser

SOURCE_URL = "https://www.yugioh-card.com/en/genesys/"
OUTPUT_FILE = "config/genesys_points.csv"
MIN_EXPECTED_CARDS = 100


class TableParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.tables = []
        self.table_stack = []
        self.row = None
        self.cell = None

    def handle_starttag(self, tag, attrs):
        tag = tag.lower()
        if tag == "table":
            self.tables.append([])
            self.table_stack.append(len(self.tables) - 1)
        elif tag == "tr" and self.table_stack:
            self.row = []
        elif tag in ("td", "th") and self.row is not None:
            self.cell = []

    def handle_data(self, data):
        if self.cell is not None:
            self.cell.append(data)

    def handle_endtag(self, tag):
        tag = tag.lower()
        if tag in ("td", "th") and self.cell is not None:
            self.row.append(" ".join("".join(self.cell).split()))
            self.cell = None
        elif tag == "tr" and self.row is not None:
            if self.table_stack:
                self.tables[self.table_stack[-1]].append(self.row)
            self.row = None
        elif tag == "table" and self.table_stack:
            self.table_stack.pop()


def main():
    request = urllib.request.Request(
        SOURCE_URL,
        headers={"User-Agent": "EDOPro-Genesys-Points-Updater/1.0"}
    )

    with urllib.request.urlopen(request, timeout=30) as response:
        html = response.read().decode("utf-8", errors="replace")

    parser = TableParser()
    parser.feed(html)

    entries = {}
    for table in parser.tables:
        for i, row in enumerate(table):
            normalized = [cell.strip().lower() for cell in row]
            if len(normalized) < 2:
                continue
            if normalized[0] != "card name" or normalized[1] != "points":
                continue

            for data_row in table[i + 1:]:
                if len(data_row) < 2:
                    continue

                name = data_row[0].strip()
                value = data_row[1].strip()

                if name and value.isdigit() and int(value) > 0:
                    entries[name] = int(value)

            if entries:
                break

        if entries:
            break

    if len(entries) < MIN_EXPECTED_CARDS:
        print(
            f"ERROR: Found only {len(entries)} point entries. "
            "The page may have changed. Existing CSV was preserved."
        )
        sys.exit(1)

    os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
    temp_file = OUTPUT_FILE + ".tmp"

    with open(temp_file, "w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["card_name", "points"])
        for name, points in sorted(
            entries.items(), key=lambda item: item[0].casefold()
        ):
            writer.writerow([name, points])

    os.replace(temp_file, OUTPUT_FILE)
    print(f"Generated {OUTPUT_FILE}: {len(entries)} cards.")


if __name__ == "__main__":
    main()
