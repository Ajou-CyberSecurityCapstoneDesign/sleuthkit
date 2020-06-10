import os, sys
import hashlib


class FileInfo:
    def __init__(self, file_path):
        self.path = file_path
        self.stat = os.stat(file_path)
        sha1 = hashlib.sha1()
        with open(self.path, 'rb') as f:
            while True:
                data = f.read(65536)
                if not data:
                    break
                sha1.update(data)
        self.sha1 = sha1.hexdigest()


def analyze_directory(directory_path):
    files_info = {}
    for dirpath, dirnames, filenames in os.walk(directory_path):
        for file_name in filenames:
            file_path = os.path.join(dirpath, file_name)
            file_info = FileInfo(file_path)
            files_info[file_info.path.replace(directory_path, '', 1)] = file_info
    return files_info


def analyze_recover(original_path, modified_path, recover_path):
    original_files_info = analyze_directory(original_path)
    modified_files_info = analyze_directory(modified_path)
    recover_files_info = analyze_directory(recover_path)

    deleted_files_original_info = {}
    for path in original_files_info:
        if path not in modified_files_info:
            print(f'Deleted file : {path}')
            deleted_files_original_info[path] = original_files_info[path]

    total_size = 0
    recovered_size = 0
    recovered_cnt = 0
    for path in deleted_files_original_info:
        original_info = deleted_files_original_info[path]
        total_size = total_size + original_info.stat.st_size
        if path in recover_files_info:
            print(f'Recovered file : {path}')
            recovered_info = recover_files_info[path]
            recovered_size = recovered_size + recovered_info.stat.st_size
            if original_info.sha1 == recovered_info.sha1:
                recovered_cnt = recovered_cnt + 1

    print(f'Deleted files : {len(deleted_files_original_info)}')
    print(f'Recovered files size : {recovered_size}/{total_size} ({round(recovered_size / total_size * 100, 2)}%)')
    print(f'Fully recovered files count : {recovered_cnt}/{len(deleted_files_original_info)} ({round(recovered_cnt / len(deleted_files_original_info) * 100, 2)}%)')


if __name__ == '__main__':
    analyze_recover(sys.argv[1], sys.argv[2], sys.argv[3])
