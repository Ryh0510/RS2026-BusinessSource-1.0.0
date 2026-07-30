import os
import sys
import shutil
import fnmatch
import yaml

#   Using --execute to delete

CONFIG_PATH = "prune_config.yaml"


def load_config(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def delete_tree(base: str, paths: list[str], dry_run: bool):
    for p in paths:
        full_path = os.path.join(base, p)
        if os.path.exists(full_path):
            if dry_run:
                print(f"[DRY-RUN] DELETE TREE: {full_path}")
            else:
                shutil.rmtree(full_path, ignore_errors=True)
                print(f"DELETED TREE: {full_path}")


def keep_only_files(base: str, rule: dict, dry_run: bool):
    target_dir = os.path.join(base, rule["dir"])
    keep_set = set(rule.get("keep", []))
    recursive = rule.get("recursive", False)

    if not os.path.isdir(target_dir):
        print(f"SKIP (not exist): {target_dir}")
        return

    for root, dirs, files in os.walk(target_dir, topdown=False):
        # 删除不在 keep 列表中的文件
        for f in files:
            if f not in keep_set:
                path = os.path.join(root, f)
                if dry_run:
                    print(f"[DRY-RUN] REMOVE FILE: {path}")
                else:
                    os.remove(path)
                    print(f"REMOVED FILE: {path}")

        # 删除所有目录（如果是 recursive 或当前目标目录）
        if recursive or root == target_dir:
            for d in dirs:
                path = os.path.join(root, d)
                if dry_run:
                    print(f"[DRY-RUN] REMOVE DIR: {path}")
                else:
                    shutil.rmtree(path, ignore_errors=True)
                    print(f"REMOVED DIR: {path}")


def remove_items(base: str, rule: dict, dry_run: bool):
    for root, dirs, files in os.walk(base, topdown=False):
        for d in dirs:
            if d in rule.get("dirs", []):
                path = os.path.join(root, d)
                if dry_run:
                    print(f"[DRY-RUN] REMOVE DIR: {path}")
                else:
                    shutil.rmtree(path, ignore_errors=True)
                    print(f"REMOVED DIR: {path}")

        for pattern in rule.get("files", []):
            for f in fnmatch.filter(files, pattern):
                path = os.path.join(root, f)
                if dry_run:
                    print(f"[DRY-RUN] REMOVE FILE: {path}")
                else:
                    os.remove(path)
                    print(f"REMOVED FILE: {path}")

def confirm_operation(base: str, dry_run: bool) -> bool:
    """
    高危操作确认
    """
    print("\n⚠️  WARNING: This operation may DELETE files/directories!")
    print(f"Project root: {base}")
    print(f"Dry run: {dry_run}\n")

    if dry_run:
        return True

    answer = input("Are you sure to CONTINUE? Type YES to proceed: ").strip()
    return answer == "YES"


def apply_rules(config: dict):
    raw_base = config["project_root"]
    base = os.path.abspath(raw_base)
    base = os.path.realpath(base)

    # ✅ CLI 覆盖 dry_run
    if "--execute" in sys.argv:
        dry_run = False
        print("⚠️  CLI override: dry_run = FALSE")
    else:
        dry_run = config.get("dry_run", True)

    print("=" * 70)
    print("PROJECT ROOT (ABSOLUTE PATH)")
    print(base)
    print("=" * 70)

    if not os.path.isdir(base):
        raise RuntimeError(f"❌ project_root does not exist: {base}")

    if not confirm_operation(base, dry_run):
        print("❌ Operation cancelled by user.")
        return

    for rule in config.get("rules", []):
        rtype = rule.get("type")

        if rtype == "delete_tree":
            delete_tree(base, rule["paths"], dry_run)

        elif rtype == "keep_only_files":
            keep_only_files(base, rule, dry_run)

        elif rtype == "remove_items":
            remove_items(base, rule, dry_run)

        else:
            print(f"UNKNOWN RULE TYPE: {rtype}")


if __name__ == "__main__":
    #   Using --execute to delete
    config = load_config(CONFIG_PATH)
    apply_rules(config)