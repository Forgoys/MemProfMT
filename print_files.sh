#!/bin/bash

# 输出文件名
output_file="all_files_content.txt"

# 清空或创建输出文件
> "$output_file"

# 递归查找所有文件并处理，忽略 .git 和 .vscode 目录
find . -type f ! -path "*.git/*"  ! -path "*.idea/*" ! -path "./build/*" ! -path "./bin/*" ! -path "print_files.sh" ! -path "README.md" ! -path "*doc/*" ! -path "*.vscode/*" ! -name "$output_file" -print0 | while IFS= read -r -d '' file; do
    # 跳过二进制文件和特殊文件
    if file "$file" | grep -q "text"; then
        # 写入文件路径
        echo "文件的相对路径：${file:2}" >> "$output_file"
        echo "文件内容：" >> "$output_file"
        # 写入文件内容
        cat "$file" >> "$output_file"
        # 写入分隔符
        echo "-------" >> "$output_file"
        echo "" >> "$output_file"
    fi
done

echo "处理完成，结果保存在 $output_file"