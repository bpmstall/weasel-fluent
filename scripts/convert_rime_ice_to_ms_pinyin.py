import os
import re

def convert_rime_dict():
    rime_dir = r"C:\Users\zheng\AppData\Roaming\Rime"
    cn_dicts_dir = os.path.join(rime_dir, "cn_dicts")
    out_dir = r"C:\Users\zheng\.gemini\antigravity\scratch\weasel-fluent-cpp-qt\scripts"
    out_file = os.path.join(out_dir, "ms_pinyin_wusong_dict.txt")
    out_curated = os.path.join(out_dir, "ms_pinyin_wusong_top50k.txt")

    dict_files = [
        os.path.join(cn_dicts_dir, "base.dict.yaml"),
        os.path.join(cn_dicts_dir, "ext.dict.yaml"),
        os.path.join(rime_dir, "custom_phrase.txt")
    ]

    seen = set()
    entries = []

    for df in dict_files:
        if not os.path.exists(df):
            print(f"Skipping {df} (not found)")
            continue
        print(f"Processing {df}...")
        with open(df, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or line.startswith("---") or line.startswith("..."):
                    continue
                parts = line.split("\t")
                if len(parts) >= 2:
                    word = parts[0].strip()
                    raw_py = parts[1].strip()
                    # Clean pinyin: remove spaces, accents, special symbols
                    py = raw_py.replace(" ", "").replace("'", "").replace("_", "").lower()
                    if not py or not word:
                        continue
                    # Only accept standard pinyin letters
                    if not re.match(r"^[a-z]+$", py):
                        continue
                    key = (py, word)
                    if key not in seen:
                        seen.add(key)
                        weight = 1
                        if len(parts) >= 3:
                            try:
                                weight = int(parts[2].strip().replace("%", ""))
                            except:
                                weight = 1
                        entries.append((py, word, weight))

    print(f"Total unique entries extracted: {len(entries)}")

    # Sort: higher weight first, then by word length
    entries.sort(key=lambda x: (-x[2], len(x[1])))

    # Write top 50,000 high-frequency phrases (ideal size for instant import into Windows 11 Microsoft Pinyin)
    top_n = min(len(entries), 50000)
    with open(out_curated, "w", encoding="utf-8") as f:
        for py, word, _ in entries[:top_n]:
            f.write(f"{py},{word},1\n")
    print(f"Exported top 50,000 phrases to: {out_curated}")

    # Write full dictionary
    with open(out_file, "w", encoding="utf-8") as f:
        for py, word, _ in entries:
            f.write(f"{py},{word},1\n")
    print(f"Exported full dictionary ({len(entries)} phrases) to: {out_file}")

if __name__ == "__main__":
    convert_rime_dict()
