import os
import csv
from typing import Dict, List, Tuple

BASE_DIR: str = "vcpkg_installed"
LICENSES_DIR = os.path.join(BASE_DIR, "licenses")


def extract_license_text(copyright_path: str) -> str:
    try:
        with open(copyright_path, "r", encoding="utf-8", errors="ignore") as file:
            return file.read().strip()
    except Exception as error:
        return f"Error reading {copyright_path}: {error}"


def collect_licenses(base_dir: str) -> Dict[str, List[Tuple[str, str]]]:
    results: Dict[str, List[Tuple[str, str]]] = {}
    
    if not os.path.isdir(base_dir):
        print(f"Dir not found: {base_dir}")
        return results

    for triplet in os.listdir(base_dir):
        share_dir = os.path.join(base_dir, triplet, "share")
        if not os.path.isdir(share_dir):
            continue

        triplet_results: List[Tuple[str, str]] = []
        for package in os.listdir(share_dir):
            copyright_file = os.path.join(share_dir, package, "copyright")
            if os.path.isfile(copyright_file):
                license_text = extract_license_text(copyright_file)
                triplet_results.append((package, license_text))

        if triplet_results:
            results[triplet] = triplet_results

    return results


def save_to_csv(licenses_data: Dict[str, List[Tuple[str, str]]]) -> None:
    if not os.path.exists(LICENSES_DIR):
        os.makedirs(LICENSES_DIR)
    
    csv_path = os.path.join(LICENSES_DIR, "licenses.csv")
    
    with open(csv_path, "w", encoding="utf-8", newline="") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(["triplet", "package", "license"])
        
        for triplet, packages in licenses_data.items():
            for package, license_text in packages:
                writer.writerow([triplet, package, license_text])


def print_licenses(licenses_data: Dict[str, List[Tuple[str, str]]]) -> None:
    for triplet, packages in licenses_data.items():
        print(f"triplet: {triplet}\n")
        print("=-------------------------=")
        
        for i, (package, license_text) in enumerate(packages):
            print(f"Package: {package}")
            print("License:")
            print(license_text if license_text else "Not found")

            if i < len(packages) - 1:
                print("\n===========\n")
        
        print("\n=-------------------------=\n")


def main() -> None:
    all_licenses = collect_licenses(BASE_DIR)

    if not all_licenses:
        print("Not found any license")
        return

    save_to_csv(all_licenses)
    print(f"Licenses saved on: {os.path.join(LICENSES_DIR, 'licenses.csv')}")

    print_licenses(all_licenses)


if __name__ == "__main__":
    main()