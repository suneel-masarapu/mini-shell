# Custom Shell in C  

A simple **Unix-like shell implementation in C** that supports command execution, piping, redirection, history, tab-completion, and more.  

## ✨ Features  
- **Basic command execution** (e.g., `ls`, `echo`, etc.)  
- **Built-in commands**  
  - `cd` → Change directory (supports `~` and `cd -`)  
  - `exit` → Exit the shell  
  - `history [n]` → Show last `n` commands  
- **Piping** (`|`) → Chain multiple commands  
- **Redirection**  
  - Input `<`  
  - Output overwrite `>`  
  - Output append `>>`  
- **Multiple command execution** with `;`  
- **Logical AND (`&&`)** execution  
- **Command history** stored in a linked list (max 2048 entries)  
- **Tab completion** for files in the current directory  
- **Signal handling**  
  - `Ctrl+C` → Exit gracefully  
  - `Ctrl+D` → EOF handling  

## ⚡ Compilation  
```bash
gcc shell.c -o myshell
```

## 🚀 Usage  
Run the shell:  
```bash
./myshell
```

You’ll see a custom prompt:  
```
shell >
```

Now try:  
```bash
ls -l
echo "Hello World"
cd ..
history 5
ls | grep ".c" > out.txt
```

## 📂 Project Structure  
- `shell.c` → Main implementation of the shell
- `instructions.pdf` ->you can try building from this

## 🛠️ Requirements  
- Linux / Unix environment  
- GCC compiler  

## 📜 Notes  
- Maximum input size: **2048 characters**  
- Supports only **foreground execution** (no background `&` yet)  
