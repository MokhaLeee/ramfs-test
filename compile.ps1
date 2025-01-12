git add .
git commit -m "compile"
gcc -g -std=c17 -O2 -I include $(Get-ChildItem -Recurse -Filter "*.c" | ForEach-Object { $_.FullName }) -o ramfs-shell