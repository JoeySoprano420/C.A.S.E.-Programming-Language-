import * as vscode from 'vscode';
import * as child_process from 'child_process';
import * as path from 'path';

export function activate(context: vscode.ExtensionContext) {
    console.log('C.A.S.E. Language extension is now active!');

    // Register compile command
    let compileCommand = vscode.commands.registerCommand('case.compile', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active C.A.S.E. file');
            return;
        }

        const document = editor.document;
        if (document.languageId !== 'case') {
            vscode.window.showErrorMessage('Current file is not a C.A.S.E. file');
            return;
        }

        // Save the document first
        await document.save();

        const config = vscode.workspace.getConfiguration('case');
        const transpilerPath = config.get<string>('transpilerPath', 'transpiler.exe');
        const showAST = config.get<boolean>('showAST', false);
        const filePath = document.fileName;

        const outputChannel = vscode.window.createOutputChannel('C.A.S.E. Compiler');
        outputChannel.show();
        outputChannel.appendLine(`Compiling ${path.basename(filePath)}...`);
        outputChannel.appendLine('');

        try {
            const result = child_process.execSync(
                `"${transpilerPath}" "${filePath}"`,
                { 
                    cwd: path.dirname(filePath),
                    encoding: 'utf8'
                }
            );

            outputChannel.appendLine(result);
            outputChannel.appendLine('');
            outputChannel.appendLine('✅ Compilation successful!');
            
            vscode.window.showInformationMessage('C.A.S.E. compilation successful!');
        } catch (error: any) {
            outputChannel.appendLine('❌ Compilation failed:');
            outputChannel.appendLine(error.stdout || error.message);
            
            vscode.window.showErrorMessage('C.A.S.E. compilation failed. Check output for details.');
        }
    });

    // Register run command
    let runCommand = vscode.commands.registerCommand('case.run', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active C.A.S.E. file');
            return;
        }

        const document = editor.document;
        const filePath = document.fileName;
        const workDir = path.dirname(filePath);
        const exePath = path.join(workDir, 'program.exe');

        // First, compile
        await vscode.commands.executeCommand('case.compile');

        // Wait a moment for compilation
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Then run
        const terminal = vscode.window.createTerminal('C.A.S.E. Program');
        terminal.show();
        terminal.sendText(`cd "${workDir}"`);
        terminal.sendText('.\\program.exe');
    });

    // Register auto-compile on save
    let saveHandler = vscode.workspace.onDidSaveTextDocument((document) => {
        const config = vscode.workspace.getConfiguration('case');
        const autoCompile = config.get<boolean>('autoCompile', false);
        
        if (autoCompile && document.languageId === 'case') {
            vscode.commands.executeCommand('case.compile');
        }
    });

    // Register diagnostic collection for errors
    const diagnosticCollection = vscode.languages.createDiagnosticCollection('case');
    context.subscriptions.push(diagnosticCollection);

    // Register hover provider
    const hoverProvider = vscode.languages.registerHoverProvider('case', {
        provideHover(document, position, token) {
            const range = document.getWordRangeAtPosition(position);
            const word = document.getText(range);

            // Keyword documentation
            const keywordDocs: { [key: string]: string } = {
                'Print': 'Outputs a value to the console.\n\nExample: `Print "Hello" [end]`',
                'let': 'Declares a variable with type inference.\n\nExample: `let x = 10 [end]`',
                'Fn': 'Defines a function.\n\nExample: `Fn add "a, b" ( ret a + b ) [end]`',
                'call': 'Calls a function.\n\nExample: `call myFunc arg1 arg2 [end]`',
                'if': 'Conditional statement.\n\nExample: `if x > 5 { Print "large" [end] } [end]`',
                'while': 'Loop statement.\n\nExample: `while i < 10 { Print i [end] } [end]`',
                'sqrt': 'Square root function.\n\nExample: `let result = sqrt 16 [end]`',
                'pow': 'Power function (x^y).\n\nExample: `let result = pow 2 8 [end]`',
                'length': 'Get string length.\n\nExample: `let len = length "text" [end]`',
                'push': 'Add element to collection.\n\nExample: `push list item [end]`',
                'thread': 'Create new thread.\n\nExample: `thread { Print "bg" [end] } [end]`',
                'http': 'Make HTTP request.\n\nExample: `http "GET" "url" result [end]`'
            };

            if (keywordDocs[word]) {
                return new vscode.Hover(new vscode.MarkdownString(keywordDocs[word]));
            }

            return undefined;
        }
    });

    // Register completion provider
    const completionProvider = vscode.languages.registerCompletionItemProvider('case', {
        provideCompletionItems(document, position, token, context) {
            const items: vscode.CompletionItem[] = [];

            // Keywords
            const keywords = [
                'Print', 'let', 'Fn', 'call', 'if', 'else', 'while', 'loop', 
                'break', 'continue', 'ret', 'switch', 'case', 'default'
            ];

            keywords.forEach(keyword => {
                const item = new vscode.CompletionItem(keyword, vscode.CompletionItemKind.Keyword);
                item.insertText = new vscode.SnippetString(keyword);
                items.push(item);
            });

            // Math functions
            const mathFuncs = ['sin', 'cos', 'tan', 'sqrt', 'pow', 'abs', 'floor', 'ceil', 'round', 'min', 'max', 'random'];
            mathFuncs.forEach(func => {
                const item = new vscode.CompletionItem(func, vscode.CompletionItemKind.Function);
                item.detail = 'Math function';
                item.insertText = new vscode.SnippetString(`${func} \${1:arg} [end]`);
                items.push(item);
            });

            // String functions
            const stringFuncs = ['length', 'substr', 'upper', 'lower', 'concat', 'split', 'trim'];
            stringFuncs.forEach(func => {
                const item = new vscode.CompletionItem(func, vscode.CompletionItemKind.Function);
                item.detail = 'String function';
                item.insertText = new vscode.SnippetString(`${func} \${1:arg} [end]`);
                items.push(item);
            });

            // Collection functions
            const collFuncs = ['push', 'pop', 'size', 'sort', 'reverse'];
            collFuncs.forEach(func => {
                const item = new vscode.CompletionItem(func, vscode.CompletionItemKind.Function);
                item.detail = 'Collection function';
                item.insertText = new vscode.SnippetString(`${func} \${1:collection} [end]`);
                items.push(item);
            });

            return items;
        }
    });

    context.subscriptions.push(compileCommand, runCommand, saveHandler, hoverProvider, completionProvider);
}

export function deactivate() {}
