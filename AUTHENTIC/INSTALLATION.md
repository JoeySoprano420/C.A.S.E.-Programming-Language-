# 🎨 C.A.S.E. IDE INTEGRATION - INSTALLATION GUIDE

## VS Code Extension Installation

### Method 1: Install from VSIX (Recommended)

1. **Package the extension:**
   ```bash
   cd vscode-extension
   npm install
   npm run compile
   npm run package
   ```

2. **Install in VS Code:**
   - Open VS Code
   - Press `Ctrl+Shift+X` (Extensions)
   - Click `...` menu (top right)
   - Select "Install from VSIX..."
   - Choose `case-language-1.0.0.vsix`

3. **Verify installation:**
   - Create a file: `test.case`
   - You should see syntax highlighting
   - Type `print` and press Tab

### Method 2: Development Mode

1. **Open extension in VS Code:**
   ```bash
   cd vscode-extension
   code .
   ```

2. **Press F5** to launch extension development host

3. **Test** in the new window

### Method 3: Manual Installation

1. **Copy extension folder:**
   - Windows: `%USERPROFILE%\.vscode\extensions\`
   - macOS/Linux: `~/.vscode/extensions/`

2. **Restart VS Code**

## Configuration

### Set Transpiler Path

1. Open Settings (`Ctrl+,`)
2. Search for "case"
3. Set **C.A.S.E.: Transpiler Path**:
   ```
   C:\path\to\transpiler.exe
   ```

### Enable Auto-Compile

1. Open Settings
2. Enable **C.A.S.E.: Auto Compile**
3. Files will compile automatically on save

## Quick Test

### Create Test File

**test.case:**
```case
Print "Hello from VS Code!" [end]

let x = 10
Print x [end]
```

### Compile & Run

1. Press `Ctrl+Shift+B` to compile
2. Check output panel
3. Press `Ctrl+Shift+R` to run

## Features to Try

### 1. Snippets

Type these and press Tab:

- `print` → Print statement
- `fn` → Function
- `if` → If statement
- `while` → While loop
- `main` → Main template

### 2. IntelliSense

- Start typing `sin` → See completion
- Press `Ctrl+Space` for suggestions

### 3. Hover Help

- Hover over keywords
- See documentation

### 4. Syntax Highlighting

- Keywords in purple
- Strings in orange
- Numbers in green
- `[end]` in red

## Keyboard Shortcuts

| Action | Windows/Linux | macOS |
|--------|---------------|-------|
| Compile | `Ctrl+Shift+B` | `Cmd+Shift+B` |
| Run | `Ctrl+Shift+R` | `Cmd+Shift+R` |
| Extensions | `Ctrl+Shift+X` | `Cmd+Shift+X` |
| Command Palette | `Ctrl+Shift+P` | `Cmd+Shift+P` |

## Troubleshooting

### Extension Not Loading

1. Check Extensions panel
2. Look for errors in Output → Log (Extension Host)
3. Reload VS Code (`Ctrl+R`)

### No Syntax Highlighting

1. Check file extension is `.case`
2. Look at bottom-right status bar
3. Click language selector → Choose "C.A.S.E."

### Compilation Fails

1. Check transpiler path in settings
2. Verify transpiler.exe exists
3. Try absolute path

### Snippets Not Working

1. Make sure file is saved as `.case`
2. Check language mode
3. Press Tab after snippet prefix

## Advanced Configuration

### settings.json

```json
{
  "case.transpilerPath": "C:\\transpiler\\transpiler.exe",
  "case.autoCompile": true,
  "case.showAST": false,
  "files.associations": {
    "*.case": "case"
  }
}
```

### keybindings.json

```json
[
  {
    "key": "f5",
    "command": "case.run",
    "when": "editorLangId == case"
  }
]
```

## Next Steps

1. ✅ Install extension
2. ✅ Configure transpiler path
3. ✅ Try snippets
4. ✅ Build first program
5. 📚 Read [Language Reference](LANGUAGE_REFERENCE.md)
6. 🎓 Follow [Tutorials](TUTORIALS.md)

---

**🌌 Violet Aura Creations - Happy Coding!** ✨
