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
            files_info[file_info.path] = file_info
    return files_info


def analyze_recover(original_path, delete_path, recover_path):
    original_files_info = analyze_directory(original_path)
    delete_files_info = analyze_directory(delete_path)
    recover_files_info = analyze_directory(recover_path)

    deleted_files_info = {}
    for file in original_files_info.values():
        if file.path not in delete_files_info:
            deleted_files_info[file.path] = file

    total_size = 0
    recovered_size = 0
    recovered_cnt = 0
    for file in deleted_files_info.values():
        total_size = total_size + file.stat.st_size
        if file.path in recover_files_info:
            recovered_size = recovered_size + file.stat.st_size
            if file.sha1 == recover_files_info[file.path].sha1:
                recovered_cnt = recovered_cnt + 1

    print(f'Deleted files : {len(deleted_files_info)}')
    print(f'Recovered files size : {recovered_size}/{total_size} ({round(recovered_size / total_size, 2)}%)')
    print(f'Fully recovered files count : {recovered_cnt}/{len(deleted_files_info)} ({round(recovered_cnt/len(deleted_files_info), 2)}%)')


if __name__ == '__main__':
    analyze_recover(sys.argv[1], sys.argv[2], sys.argv[3])
