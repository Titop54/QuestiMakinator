import os
import csv
import sys
from typing import Dict, List, Tuple

BASE_DIR: str = "vcpkg_installed"
LICENSES_DIR = os.path.join(BASE_DIR, "licenses")

def print_help() -> None:
    """Prints usage information similar to the bash script style."""
    print(f"Usage: python3 {sys.argv[0]} [OPTIONS] [PACKAGES...]\n")
    print("Options:")
    print("  --no, --no-print, --no_print    Skip printing license text to the console.")
    print("  -h, --help                      Show this help message and exit.\n")
    print("Arguments:")
    print("  PACKAGES                        List of vcpkg ports (supports features like package[feature]).")
    print("                                  Example: imgui[glfw-binding] glfw3\n")

def extract_license_text(copyright_path: str) -> str:
    """Reads the copyright file content."""
    try:
        with open(copyright_path, "r", encoding="utf-8", errors="ignore") as file:
            return file.read().strip()
    except Exception as error:
        return f"Error reading {copyright_path}: {error}"

def collect_licenses(base_dir: str, target_packages: List[str]) -> Dict[str, List[Tuple[str, str]]]:
    """Collects licenses handling the specific x-install-root nested structure."""
    results: Dict[str, List[Tuple[str, str]]] = {}
    
    #"imgui[glfw-binding]" -> "imgui"
    cleaned_targets = {pkg.split('[')[0].strip() for pkg in target_packages}
    
    if not os.path.isdir(base_dir):
        return results

    for custom_root in os.listdir(base_dir):
        custom_root_path = os.path.join(base_dir, custom_root)
        
        if custom_root == "licenses" or not os.path.isdir(custom_root_path):
            continue

        for subfolder in os.listdir(custom_root_path):
            triplet_path = os.path.join(custom_root_path, subfolder)
            share_dir = os.path.join(triplet_path, "share")
            
            if not os.path.isdir(share_dir):
                continue

            triplet_results: List[Tuple[str, str]] = []
            
            for folder_name in os.listdir(share_dir):
                if folder_name in cleaned_targets:
                    copyright_file = os.path.join(share_dir, folder_name, "copyright")
                    if os.path.isfile(copyright_file):
                        license_text = extract_license_text(copyright_file)
                        triplet_results.append((folder_name, license_text))

            if triplet_results:
                results[custom_root] = triplet_results

    return results

def save_to_csv(licenses_data: Dict[str, List[Tuple[str, str]]]) -> None:
    """Saves the collected data to a CSV file."""
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
    """Displays the licenses in the console."""
    for triplet, packages in licenses_data.items():
        print(f"triplet: {triplet}\n")
        print("=-----------------------------------------------=")
        
        for i, (package, license_text) in enumerate(packages):
            print(f"Package: {package}")
            print("License:")
            print(license_text if license_text else "Not found")

            if i < len(packages) - 1:
                print("\n===========\n")
        
        print("\n=-----------------------------------------------=\n")

def main() -> None:
    raw_args = sys.argv[1:]

    if "-h" in raw_args or "--help" in raw_args:
        print_help()
        return

    silent_flags = {"--no", "--no-print", "--no_print"}
    no_print = any(flag in raw_args for flag in silent_flags)
    
    target_libs = [arg for arg in raw_args if not arg.startswith("-")]

    if not target_libs:
        print("Error: No packages specified. Use --help for usage.")
        return

    all_licenses = collect_licenses(BASE_DIR, target_libs)

    if not all_licenses:
        print(f"No licenses found in {BASE_DIR} for the specified packages.")
        return

    save_to_csv(all_licenses)
    print(f"Licenses saved on: {os.path.join(LICENSES_DIR, 'licenses.csv')}")

    if not no_print:
        print_licenses(all_licenses)

if __name__ == "__main__":
    main()