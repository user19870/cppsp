const vscode = require('vscode');

let structVariables = [];  
let functions = [];       // [functionName]
let customs=[];
const keywords = ["if", "else", "while", "for","use","import","var","function","struct"
  ,"package"
]; // 控制關鍵字
const flags=["#useclang","#usegcc","#overwrite","#skipcompile"];
const ratsmark=["@custom"];

function activate(context) {

    const legend = new vscode.SemanticTokensLegend(["type"]);
    const semanticProvider = {
    provideDocumentSemanticTokens(document) {

        const builder = new vscode.SemanticTokensBuilder(legend)

        if(structVariables.length === 0) return builder.build()

        const regex = new RegExp(`\\b(${structVariables.join("|")})\\b`, "g")

        for (let line = 0; line < document.lineCount; line++) {
            const text = document.lineAt(line).text

            let match
            while ((match = regex.exec(text))) {
                builder.push(line, match.index, match[0].length, 0, 0)
            }
        }

        return builder.build()
    }
}
context.subscriptions.push(
    vscode.languages.registerDocumentSemanticTokensProvider(
        { language: 'cppsp' },
        semanticProvider,
        legend
    )
)

    // Completion Provider
    const provider = vscode.languages.registerCompletionItemProvider(
        'cppsp', // 你的 language id
        {

            
            provideCompletionItems(document, position) {
 

                const items = [];
                // --- 1. Keywords ---
                 for (const kw of keywords) {
                    const item = new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword);
                    items.push(item);
                }

                // --- 2. Functions ---
                for (const f of functions) {
                    const item = new vscode.CompletionItem(f, vscode.CompletionItemKind.Function);
                    item.insertText = new vscode.SnippetString(f + "($1)");
                    items.push(item);
                }
                for (const cs of customs) {
                    const item = new vscode.CompletionItem(cs, vscode.CompletionItemKind.Function);
                    item.insertText = new vscode.SnippetString(cs + "($1)");
                    items.push(item);
                }

                // --- 3. Struct members ---
                for (const structmem of structVariables) {
                        const item = new vscode.CompletionItem(structmem, vscode.CompletionItemKind.Class);
                        items.push(item);
                }

                // --- 4. API example ---
                const printItem = new vscode.CompletionItem("print", vscode.CompletionItemKind.Function);
                printItem.insertText = new vscode.SnippetString("print($1)");
               // printItem.detail = "Print API";
                items.push(printItem);
                const inputItem = new vscode.CompletionItem("input", vscode.CompletionItemKind.Function);
                inputItem.insertText = new vscode.SnippetString("input($1)");
                items.push(inputItem);
                const maininject = new vscode.CompletionItem("@inject",vscode.CompletionItemKind.Snippet)
                maininject.insertText =new vscode.SnippetString("inject(\"$1\")")
                items.push(maininject);
                const globalinject = new vscode.CompletionItem("@function",vscode.CompletionItemKind.Snippet)
                globalinject.insertText =new vscode.SnippetString("function<<$1>>")
                items.push(globalinject);
                const cppcommand = new vscode.CompletionItem("@command",vscode.CompletionItemKind.Snippet)
                cppcommand.insertText =new vscode.SnippetString("command(\"$1\")")
                items.push(cppcommand);
                const namespacs = new vscode.CompletionItem("namespace",vscode.CompletionItemKind.Snippet)
                namespacs.insertText =new vscode.SnippetString("namespace $1{$2}")
                items.push(namespacs);
                for(const fg of flags){
                    const fgtemp =new vscode.CompletionItem(fg, vscode.CompletionItemKind.Keyword);
                    fgtemp.insertText =fg.slice(1);
                    items.push(fgtemp);
                }
                for(const rm of ratsmark){
                    const rmtemp =new vscode.CompletionItem(rm, vscode.CompletionItemKind.keywords);
                    rmtemp.insertText =rm.slice(1);
                    items.push(rmtemp);
                }
                

                return items;
            }
        }, "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z",
         "@","#"
        // 不設 trigger character，自動 Ctrl+Space 呼叫
    );

    context.subscriptions.push(provider);

    // --- Text change listener for tracking structs/functions ---
    vscode.workspace.onDidChangeTextDocument((event) => {
        const lines = event.document.getText().split(/\r?\n/);
        structVariables = []; // reset for simplicity
        functions = [];
        customs=[];

        for (const line of lines) {
            const structMatch = line.match(/struct\s+([a-zA-Z_][a-zA-Z0-9_]*(?:\s+[a-zA-Z_][a-zA-Z0-9_]*)*)/);
if (structMatch) {
    const names = structMatch[1].split(/\s+/); // 分割多個名稱
    for (const name of names) {
        const cleanName = name.replace(/[{}]/g,''); // 去掉可能的 {}
        if (cleanName && !structVariables.includes(cleanName)) {
            structVariables.push(cleanName);
        }
    }
}

            
            
            const funcArrayMatch = line.match(/function\s*\[\s*([a-zA-Z0-9_,\s]+)\s*\]/);
            if (funcArrayMatch) {
                // 取出中括號內的內容
                 const names = funcArrayMatch[1].split(",").map(n => n.trim()).filter(n => n);
                 for (const name of names) {
                    if (!functions.includes(name)) {
                        functions.push(name);
                    }
                }
            } else{
                const funcMatch = line.match(/function\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(/);
            if (funcMatch) {
                const funcName = funcMatch[1];
                if (!functions.includes(funcName)) {
                    functions.push(funcName);
                }
            }
            }
            const customMatch = line.match(/@custom\s+([a-zA-Z_@][a-zA-Z0-9_@]*)\s*\(/);
            if (customMatch) {
                const cusName = customMatch[1];
                if (!customs.includes(cusName)) {
                    customs.push(cusName);
                }
            }
        }
    });
}

function deactivate() {}

module.exports = { activate, deactivate };