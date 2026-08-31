/**
 * LVLang JavaScript / Node.js SDK
 * Allows executing LVLang 2-byte hex streams directly from Node.js applications.
 */

const { execFileSync } = require('child_process');
const path = require('path');

const LVLC_PATH = path.join(__dirname, 'lvlc');

class LVLang {
    /**
     * Executes a 2-byte hex instruction stream directly in LVLang VM.
     * @param {string} hexStream 
     * @returns {string} Clean stdout output
     */
    static eval(hexStream) {
        try {
            const stdout = execFileSync(LVLC_PATH, [hexStream], { encoding: 'utf8' });
            const outputMarker = "=== Executing in LVLang VM Runtime ===";
            if (stdout.includes(outputMarker)) {
                let rawOutput = stdout.split(outputMarker)[1];
                if (rawOutput.includes("[VM Halted]")) {
                    rawOutput = rawOutput.split("[VM Halted]")[0];
                }
                rawOutput = rawOutput.replace(/^\n?Output:\s?/, '');
                return rawOutput.trim();
            }
            return stdout.trim();
        } catch (err) {
            throw new Error(`LVLang Execution Error: ${err.message}`);
        }
    }
}

module.exports = LVLang;

if (require.main === module) {
    console.log("[+] Testing LVLang Node.js SDK Execution...");
    const res = LVLang.eval("01xE4 0Ax05 01xB2 0Ax05 0Ax04 0Ax08 0Ax07 05x04 05xFF");
    console.log(`[+] Output from Node.js SDK: ${res}`);
}
