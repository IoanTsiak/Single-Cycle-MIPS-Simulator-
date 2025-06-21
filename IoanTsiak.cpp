#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <iomanip>
#include <bitset>

using namespace std;
//===================IMPORTANT!!! READ THIS!!!===================
//I added comments to help you understand it.
//If you want to run the code, make sure to write the correct form in the console. For example,
//if you want to print cycles 30, 34, and the final state, just write 30,34,last (where last stands for the final state).
// -------------------------------------------------------------------
// =================== Control Signals Struct ========================
// -------------------------------------------------------------------
// This struct holds all the single-cycle control signals that typically
// come from the "control unit" in a MIPS CPU.
// - RegisterDestination: If true, write destination is 'rd' (R-type).
// - JumpSignal: If true, a jump instruction is executing.
// - BranchSignal: If true, a branch instruction (beq, bne) is executing.
// - MemoryRead: If true, read from memory (lw).
// - MemoryToRegister: If true, take data from memory to write into a register (lw).
// - ALUOperation: The numeric code that helps define the ALU function.
// - MemoryWrite: If true, write data to memory (sw).
// - ALUSource: If true, second ALU input is an immediate (I-type).
// - RegisterWrite: If true, write back into a register.

struct ControlSignals {
    bool RegisterDestination;
    bool JumpSignal;
    bool BranchSignal;
    bool MemoryRead;
    bool MemoryToRegister;
    int  ALUOperation;
    bool MemoryWrite;
    bool ALUSource;
    bool RegisterWrite;

    ControlSignals() {
        RegisterDestination=false;
        JumpSignal=false;
        BranchSignal=false;
        MemoryRead=false;
        MemoryToRegister=false;
        ALUOperation=0;
        MemoryWrite=false;
        ALUSource=false;
        RegisterWrite=false;
    }


    string ToBinaryString() const {

        int op = (ALUOperation & 0b11);
        ostringstream oss;
        oss << (RegisterDestination ? '1':'0') << " "
            << (JumpSignal          ? '1':'0') << " "
            << (BranchSignal        ? '1':'0') << " "
            << (MemoryRead          ? '1':'0') << " "
            << (MemoryToRegister    ? '1':'0') << " "
            << bitset<2>(op)        << " "
            << (MemoryWrite         ? '1':'0') << " "
            << (ALUSource           ? '1':'0') << " "
            << (RegisterWrite       ? '1':'0');
        return oss.str();
    }
};

// -------------------------------------------------------------------
// =================== Instruction Struct ============================
// -------------------------------------------------------------------
// This struct encapsulates one assembly instruction's essential fields:
// - label: if there's a label on the same line
// - opcode: e.g. "add", "lw", "beq", etc.
// - rs, rt, rd: integer register numbers for the MIPS fields
// - imm: 32-bit immediate or offset
// - originalLine: the exact text string of this instruction for printing
struct Instruction {
    string label;
    string opcode;
    int rs=0, rt=0, rd=0;
    int32_t imm=0;
    string originalLine;
};

// -------------------------------------------------------------------
// =================== Register File ===============================
// -------------------------------------------------------------------
// Holds the 32 registers (regs[]) plus the program counter (pc).
// We initialize all registers to zero, including pc.
// Typically, $gp is set to 0x10008000, $sp is set to 0x7ffffffc later.
struct RegFile {
    int32_t regs[32];
    uint32_t pc;
    RegFile() {
        memset(regs, 0, sizeof(regs));
        pc = 0;
    }
};

// -------------------------------------------------------------------
// =================== Sparse Memory ===============================
// -------------------------------------------------------------------
// A simple "sparse" memory model using an unordered_map. We store only
// addresses that are written or read, so we don't need a huge array.
struct SparseMem {
    unordered_map<uint32_t,int32_t> data;
};

// -------------------------------------------------------------------
// GetAlu2Bits: Returns a 2-bit string code for the ALU operation that
// will be displayed in the control signals portion of the cycle printout.
// -------------------------------------------------------------------
static string GetAlu2Bits(const string &opc)
{
    if (opc=="and" || opc=="andi" || opc=="nor"|| opc=="lw"|| opc=="sw") {
        return "00";
    }
    else if (opc=="or" || opc=="ori"||opc=="beq"||opc=="bne") {
        return "01";
    }
    else if (opc=="add"||opc=="addu"||opc=="addi"||opc=="addiu"||
             opc=="sub"||opc=="subu"||opc=="sll"||opc=="srl") {
        return "10";
    }
    else if (opc=="slt"||opc=="slti"||opc=="sltu"||opc=="sltiu") {
        return "11";
    }
    return "--";
}

// -------------------------------------------------------------------
// =================== SingleCycleMIPS Class =========================
// -------------------------------------------------------------------
// Orchestrates the loading of assembly instructions, simulates them
// in a single-cycle manner, and prints out the cycle-by-cycle (or
// final) state as requested.
class SingleCycleMIPS {
public:

    void LoadAssembly(const string &filename);                         // read instructions from file
    void RunSimulation(const string &outFile, const vector<int> &cyclesToPrint, bool includeLast);

private:
    // Data fields:

    // The CPU state:
    RegFile    rf;      // The 32 registers + PC
    SparseMem  mem;     // Sparse memory structure
    vector<Instruction> instrMem; // The instruction memory (vector)
    unordered_map<string,int> labelMap; // label -> instruction index

    // For monitoring/printing each cycle:
    bool didLoad=false, didStore=false;
    uint32_t memAddress=0;
    int32_t storeValue=0, loadValue=0;

    int cycleCount=0;   // how many instructions (cycles) have executed
    bool finished=false; // true when the program ends

    // The current instruction being executed:
    Instruction IR;
    int32_t regA=0, regB=0;
    int32_t aluOut=0;
    int32_t memDataReg=0;

    // The control signals for the current instruction:
    ControlSignals ctrl;
    string branchLabelMonitor = "-";

    // A list of memory addresses modified in the current cycle
    vector<uint32_t> changedAddrs;

private:
    // ---------- Parsing-related ----------
    void Trim(string &s);                        // remove leading/trailing whitespace
    void ParseLine(const string &line, Instruction &ins, string &lbl);
    void ParseInstruction(const string &text, Instruction &ins);
    int  ParseRegister(string token);

    // ---------- Helpers ----------
    bool IsHalt(const Instruction &ins);         // detect "sll $zero,$zero,0" as a halt

    // ---------- Control & Execution ----------
    void GenerateControl(const string &opcode);  // sets the control signals
    int32_t DoALU(const string &opcode, int32_t a, int32_t b);  // performs the ALU operation
    void ExecuteInstruction(Instruction &ins);   // runs one instruction in one cycle

    // ---------- Printing / Logging ----------
    void PrintCycleInformation(std::ostream &out, uint32_t oldPC); // print cycle-by-cycle info
    void PrintFinalState(ostream &out);          // print final registers/memory/cycle count
    void DecodeMonitorRegisters(const Instruction &ins, string &m3, string &m4, string &m5);
};

// -------------------------------------------------------------------
// LoadAssembly: Reads lines from a file, parsing them into instructions.
//               Also identifies labels and stores them in labelMap.
//
// Halts early if it finds the "sll $zero, $zero, 0" instruction.
// -------------------------------------------------------------------
void SingleCycleMIPS::LoadAssembly(const string &filename) {
    ifstream fin(filename);
    if(!fin.is_open()){
        cerr<<"Cannot open "<<filename<<"\n";
        return;
    }
    string line;
    while(getline(fin,line)) {
        // 1) Print the raw line:
        cout << "[DEBUG] Raw line read: '" << line << "'\n";
        // Remove any comment starting with '#'
        auto cpos = line.find('#');
        if(cpos!=string::npos){
            line=line.substr(0,cpos);
        }
        Trim(line);
        if (line.empty()) {
            cout << "[DEBUG] => line is empty after trim\n\n";
            continue;
        }

        // 2) Print line after trim/comment removal:
        cout << "[DEBUG] => after trim: '" << line << "'\n";

        // Some lines might have .data or .text - we just ignore them
        {
            istringstream iss(line);
            vector<string> tokens;
            string t;
            while(iss >> t) tokens.push_back(t);
            if(tokens.empty()) continue;
            if(tokens[0]==".data"||tokens[0]==".text") {
                // ignore
                continue;
            }
        }

        // Now parse the instruction
        Instruction ins;
        ins.originalLine=line;
        string lbl;
        ParseLine(line, ins, lbl);

        // If we see sll $zero, $zero, 0, we treat that as a "halt"
        // and stop reading further lines.
        if (ins.opcode == "sll" && ins.rs == 0 && ins.rt == 0 && ins.rd == 0 && ins.imm == 0) {
            cout << "Halt instruction encountered. Stopping further input.\n";
            instrMem.push_back(ins);  // Add the halt instruction into the instruction memory.
            break;
        }

        // If there's a valid opcode, push back the instruction
        if(!ins.opcode.empty()){
            // If there's a label on the same line, map label -> current index
            if(!lbl.empty()){
                labelMap[lbl] = (int)instrMem.size();
            }
            instrMem.push_back(ins);
        } else {
            // Otherwise, maybe it was just a label line.
            if(!lbl.empty()){
                labelMap[lbl] = (int)instrMem.size();
            }
        }
    }
    fin.close();
}

// -------------------------------------------------------------------
// RunSimulation: Executes instructions 1 cycle at a time until we
//                either "run out" of instructions or see the halt.
//
//  - outFile: name of the output file to create
//  - cyclesToPrint: which cycle numbers to log
//  - includeLast: if true, we also print final registers/memory after
//    the simulation ends
// -------------------------------------------------------------------
void SingleCycleMIPS::RunSimulation(const string &outFile, const vector<int> &cyclesToPrint, bool includeLast) {
    ofstream out(outFile);
    if(!out.is_open()){
        cerr<<"Cannot open "<<outFile<<"\n";
        return;
    }
    // Print
    out<<"Name: *****\nUniversity ID: *****\n\n";

    // Initialize registers
    memset(rf.regs,0,sizeof(rf.regs));
    // Typical GP and SP initialization that are given
    rf.regs[28]=0x10008000; // $gp
    rf.regs[29]=0x7ffffffc; // $sp
    rf.pc=0;

    cycleCount=0;
    finished=false;

    // Main loop: fetch instruction at pc/4, then execute
    while(!finished) {
        // 1) Capture oldPC BEFORE  execute the instruction
        uint32_t oldPC = rf.pc;
    
        uint32_t idx = oldPC/4;

        if(idx >= instrMem.size()) {
            break;
        }
        Instruction &ins = instrMem[idx];
        IR = ins;
    
        cycleCount++;

        // Reset the monitoring for this cycle
        didLoad=false;
        didStore=false;
        memAddress=0;
        storeValue=0;
        loadValue=0;
        branchLabelMonitor="-";
        aluOut=0;
        memDataReg=0;
        regA=0;
        regB=0;
        changedAddrs.clear();

        // Single-cycle logic
        ExecuteInstruction(ins);

        // If this cycle is one the user asked to print (or if "all"),
        // we log it
        bool shouldPrint = false;
        if (finished) {
            // If we just executed the halt (final) cycle, print its monitor info only if the user
            // explicitly requested that cycle number.
            if (find(cyclesToPrint.begin(), cyclesToPrint.end(), cycleCount) != cyclesToPrint.end())
                shouldPrint = true;
        } else {
            if (find(cyclesToPrint.begin(), cyclesToPrint.end(), cycleCount) != cyclesToPrint.end() ||
                find(cyclesToPrint.begin(), cyclesToPrint.end(), -1) != cyclesToPrint.end())
                shouldPrint = true;
        }
        
        if (shouldPrint)
            PrintCycleInformation(out, oldPC);
    }

    // If user wants final snapshot, print it now
    if (includeLast){
        PrintFinalState(out);
    }

    out.close();
}

// -------------------------------------------------------------------
// ExecuteInstruction: The main single-cycle logic for one instruction.
// 1) Determine control signals
// 2) Read register file
// 3) Possibly sign/zero-extend immediate
// 4) Handle jumps/branches
// 5) ALU
// 6) Memory read/write
// 7) Write back
// 8) PC increment
// -------------------------------------------------------------------
void SingleCycleMIPS::ExecuteInstruction(Instruction &ins) {
    // Step 1: Control
    GenerateControl(ins.opcode);

    // Step 2: read regs
    regA = rf.regs[ins.rs];
    regB = rf.regs[ins.rt];

    // Step 3: immediate extension if I-type
    if(ins.opcode=="addi"||ins.opcode=="addiu"||ins.opcode=="andi"||
       ins.opcode=="ori" ||ins.opcode=="slti"||ins.opcode=="sltiu"){
        // andi, ori => zero-extend
        if(ins.opcode=="andi"||ins.opcode=="ori"){
            uint16_t tmp = (uint16_t)(ins.imm & 0xffff);
            ins.imm=(int32_t)tmp;
        }
        // otherwise => sign-extend
        else {
            int16_t tmp = (int16_t)(ins.imm & 0xffff);
            ins.imm=(int32_t)tmp;
        }
    }

    // Step 4: handle j, beq, bne (which modify PC directly)
    if(ins.opcode=="j"){
        ctrl.JumpSignal=true;

        // Debug print: show the current PC, the jump, and where we go
        cout << "[DEBUG] (PC=0x" << hex << rf.pc << ") j " 
        << ins.label << "\n";

        if(!ins.label.empty()){
            if(labelMap.find(ins.label)!=labelMap.end()){
                uint32_t targetPC = labelMap[ins.label]*4;
                cout << "       => Jumping to PC=0x" << targetPC << "\n";
                rf.pc = targetPC;
            } else {
                cout << "       => label not found => finishing.\n";
                finished=true;
            }
        }
        return;
    }

    if(ins.opcode=="beq"){
        ctrl.BranchSignal=true;
        branchLabelMonitor = ins.label;

          // Debug: show current PC, the condition, and result
          cout << "[DEBUG] (PC=0x" << hex << rf.pc << ") beq: regA=0x" 
          << regA << ", regB=0x" << regB << "\n";

        if(regA==regB){
            // Branch taken
            cout << "       => condition is TRUE => branch TAKEN\n";
            
            if(!ins.label.empty()){
                if(labelMap.find(ins.label)!=labelMap.end()){
                    uint32_t targetPC = labelMap[ins.label]*4;
                    cout << "       => new PC=0x" << targetPC << "\n";
                    rf.pc = targetPC;
                } else {
                    cout << "       => label not found => finishing.\n";
                    finished=true;
                }
            }
        } else {
            cout << "       => NOT taken => next PC=0x" 
                 << (rf.pc+4) << "\n";
            rf.pc+=4;
        }
        return;
    }

    if(ins.opcode=="bne"){
        ctrl.BranchSignal=true;
        branchLabelMonitor = ins.label;

        // Debug
        cout << "[DEBUG] (PC=0x" << hex << rf.pc << ") bne: regA=0x" 
             << regA << ", regB=0x" << regB << "\n";

        if(regA!=regB){
            cout << "       => condition is TRUE => branch TAKEN\n";

            if(!ins.label.empty()){
                if(labelMap.find(ins.label)!=labelMap.end()){
                    uint32_t targetPC = labelMap[ins.label]*4;
                    cout << "       => new PC=0x" << targetPC << "\n";
                    rf.pc = targetPC;
                } else {
                    cout << "       => label not found => finishing.\n";
                    finished=true;
                }
            }
        } else {
            cout << "       => NOT taken => next PC=0x" 
                 << (rf.pc+4) << "\n";
            rf.pc+=4;
        }
        return;
    }

    // Step 5: normal ALU
    if (ins.opcode == "sll" || ins.opcode == "srl") {
        // For shift instructions, use the value from register rt and the shift amount from ins.imm.
        aluOut = DoALU(ins.opcode, rf.regs[ins.rt], ins.imm);
    } else {
        int32_t srcB = (ctrl.ALUSource ? ins.imm : regB);
        aluOut = DoALU(ins.opcode, regA, srcB);
    }

    // Step 6: memory ops
    if(ins.opcode=="lw"){
        ctrl.MemoryRead=true;
        didLoad=true;
        memAddress= aluOut;
        // If address not in map, default 0
        if(mem.data.find(aluOut)==mem.data.end()){
            memDataReg=0;
        } else {
            memDataReg=mem.data[aluOut];
        }
    }
    else if(ins.opcode=="sw"){
        ctrl.MemoryWrite=true;
        didStore=true;
        memAddress= aluOut;
        storeValue= regB;
        mem.data[aluOut] = regB;       // store
        changedAddrs.push_back(aluOut);
    }

    // Step 7: write back
    // If it's the "halt" instruction, we stop
    if(IsHalt(ins)){
        finished = true;
        rf.pc += 4;  // increment PC for the halt cycle so that final PC becomes PC+4
        return;
    }
   // R-type: for sltu (and for slt, if needed) we want the boolean in the register
// but the ALU out to be the raw subtraction result.
if (ins.opcode == "sltu") {
    int32_t rawSub = regA - regB;  // raw subtraction result
    int32_t boolRes = ((uint32_t)regA < (uint32_t)regB) ? 1 : 0;
    rf.regs[ins.rd] = boolRes;     // store boolean in destination register
    aluOut = rawSub;             // use raw subtraction for monitor output
} else if (ins.opcode == "slt") {
    int32_t rawSub = regA - regB;
    int32_t boolRes = (regA < regB) ? 1 : 0;
    rf.regs[ins.rd] = boolRes;
    aluOut = rawSub;
} else if (ins.opcode=="add" || ins.opcode=="addu" || ins.opcode=="sub" || ins.opcode=="subu" ||
           ins.opcode=="and" || ins.opcode=="or"  || ins.opcode=="nor" ||
           ins.opcode=="sll" || ins.opcode=="srl") {
    rf.regs[ins.rd] = aluOut;
}
    
    // I-type => write to rt
    else if(ins.opcode=="addi"||ins.opcode=="addiu"||ins.opcode=="andi"||
            ins.opcode=="ori" ||ins.opcode=="slti"||ins.opcode=="sltiu"){
        rf.regs[ins.rt] = aluOut;}
    
    // lw => load data into rt
    else if(ins.opcode=="lw"){
        rf.regs[ins.rt] = memDataReg;
    }

    // Step 8: increment PC
    rf.pc +=4;
}

// -------------------------------------------------------------------
// GenerateControl: sets the single-cycle control signals based on opcode
// -------------------------------------------------------------------
void SingleCycleMIPS::GenerateControl(const string &opc){
    // Reset all signals
    ctrl = ControlSignals();

    // R-type (like add, sub, sll, etc.)
    if(opc=="add"||opc=="addu"||opc=="sub"||opc=="subu"||
       opc=="and"||opc=="or"||opc=="nor"||opc=="slt"||
       opc=="sltu"){
        ctrl.RegisterDestination=true;  // writes to rd
        ctrl.ALUOperation=2;           // 2 means "R-type function" in doALU
        ctrl.ALUSource=false;
        ctrl.RegisterWrite=true;
    }
    else if (opc=="sll" || opc=="srl") {
        // For shift instructions, the expected control signals are:
        // RegisterDestination = 0, ALUOperation = 2, ALUSource = 1, RegisterWrite = 1.
        ctrl.RegisterDestination = false; 
        ctrl.ALUOperation = 2;
        ctrl.ALUSource = true;
        ctrl.RegisterWrite = true;
        
    }
    // I-type (addi, andi, slti, etc.)
    else if(opc=="addi"||opc=="addiu"||opc=="andi"||opc=="ori"||
            opc=="slti"||opc=="sltiu"){
        ctrl.RegisterDestination=false; // writes to rt
        ctrl.ALUOperation=0;           // 0 => add-like
        ctrl.ALUSource=true;
        ctrl.RegisterWrite=true;
    }
    // lw => read from memory
    else if(opc=="lw"){
        ctrl.ALUSource=true;
        ctrl.MemoryToRegister=true;
        ctrl.RegisterWrite=true;
        ctrl.MemoryRead=true;
        ctrl.ALUOperation=0;  // add
    }
    // sw => store to memory
    else if(opc=="sw"){
        ctrl.ALUSource=true;
        ctrl.MemoryWrite=true;
        ctrl.ALUOperation=0; // add
    }
    // beq, bne => sub
    else if(opc=="beq"||opc=="bne"){
        ctrl.BranchSignal=true;
        ctrl.ALUOperation=1; // sub
    }
    // j => jump
    else if(opc=="j"){
        ctrl.JumpSignal=true;
    }
}

// -------------------------------------------------------------------
// DoALU: performs the ALU operation based on the opcode
// -------------------------------------------------------------------
int32_t SingleCycleMIPS::DoALU(const string &opc, int32_t a, int32_t b){
    if(opc=="add"||opc=="addi"||opc=="addiu") return a+b;
    if(opc=="addu") return (uint32_t)a + (uint32_t)b;
    if(opc=="sub"||opc=="subu") return a-b;
    if(opc=="and"||opc=="andi") return (a & b);
    if(opc=="or" ||opc=="ori")  return (a | b);
    if(opc=="nor")              return ~(a | b);
    if(opc=="slt")              return (a<b) ? 1 : 0;
    if(opc=="sltu")             return ((uint32_t)a<(uint32_t)b)?1:0;
    if(opc=="slti")             return (a<b)?1:0;
    if(opc=="sltiu")            return ((uint32_t)a<(uint32_t)b)?1:0;
    if (opc == "sll") return ((uint32_t)a << (b & 31));
    if (opc == "srl") return ((uint32_t)a >> (b & 31));
    // default => just do add
    return a+b;
}

// -------------------------------------------------------------------
// Trim: removes leading/trailing whitespace in a string
// -------------------------------------------------------------------
void SingleCycleMIPS::Trim(string &s){
    while(!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    while(!s.empty() && isspace((unsigned char)s.back()))  s.pop_back();
}

// -------------------------------------------------------------------
// ParseLine: checks if there's a label ("something:") in the line,
//            then parses the rest as an instruction.
//
// e.g. "start: addi $t0, $zero, 5"
// -------------------------------------------------------------------
void SingleCycleMIPS::ParseLine(const string &line, Instruction &ins, string &lbl){
    auto p=line.find(':');
    if(p!=string::npos){
        lbl=line.substr(0,p);
        Trim(lbl);
        string after=line.substr(p+1);
        Trim(after);
        if(!after.empty()){
            ParseInstruction(after,ins);
        }
    } else {
        // no label, just parse
        ParseInstruction(line,ins);
    }
}

// -------------------------------------------------------------------
// ParseInstruction: splits the text into tokens and interprets them
// as a MIPS instruction format (e.g. opcode, registers, immediate)
// -------------------------------------------------------------------
void SingleCycleMIPS::ParseInstruction(const string &text, Instruction &ins){


        // Make a copy of 'text'
        string line = text;
        // 1) Replace all commas with spaces:
        for (char &c : line) {
            if (c == ',') {
                c = ' ';
            }
        }

    vector<string> tokens;
    {
        istringstream iss(line);
        string t;
        while(iss >> t) tokens.push_back(t);
    }
    if(tokens.empty()) return;
    ins.opcode = tokens[0];

    // j
    if(ins.opcode=="j"){
        if(tokens.size()>=2){
            ins.label = tokens[1];
        }
        return;
    }
    // beq, bne => beq $rs, $rt, LABEL
    if(ins.opcode=="beq"||ins.opcode=="bne"){
        if(tokens.size()>=4){
            ins.rs = ParseRegister(tokens[1]);
            ins.rt = ParseRegister(tokens[2]);
            ins.label= tokens[3];
        }
        return;
    }
    // Shift instructions: sll and srl have a different operand format.
// Format: sll $rd, $rt, shamt
if(ins.opcode == "sll" || ins.opcode == "srl"){
    if(tokens.size() < 4) return;
    ins.rd = ParseRegister(tokens[1]);              // destination register
    ins.rt = ParseRegister(tokens[2]);              // register to shift
    ins.imm = stoi(tokens[3], nullptr, 0);            // shift amount (immediate)
    ins.rs = 0;                                     // not used in shift instructions
    return;
}

// R-type instructions (non-shift): add, addu, sub, subu, and, or, nor, slt, sltu
if(ins.opcode=="add" || ins.opcode=="addu" || ins.opcode=="sub" || ins.opcode=="subu" ||
   ins.opcode=="and" || ins.opcode=="or"  || ins.opcode=="nor" || ins.opcode=="slt" ||
   ins.opcode=="sltu"){
    if(tokens.size() < 4) return;
    ins.rd = ParseRegister(tokens[1]);
    ins.rs = ParseRegister(tokens[2]);
    ins.rt = ParseRegister(tokens[3]);
    return;
}

    // I-type => addi $rt, $rs, IMM
    if(ins.opcode=="addi"||ins.opcode=="addiu"||ins.opcode=="andi"||ins.opcode=="ori"||
       ins.opcode=="slti"||ins.opcode=="sltiu"){
        if(tokens.size()<4) return;
        ins.rt=ParseRegister(tokens[1]);
        ins.rs=ParseRegister(tokens[2]);
        // Parse imm with base=0 so 0xNNN works
        ins.imm= stoi(tokens[3],nullptr,0);        return;
    }
    // lw, sw => lw $rt, offset($rs)
    if(ins.opcode=="lw"||ins.opcode=="sw"){
        if(tokens.size()<3) return;
        ins.rt=ParseRegister(tokens[1]);
        string expr=tokens[2];
        auto p1=expr.find('(');
        auto p2=expr.find(')');
        if(p1!=string::npos && p2!=string::npos){
            string off=expr.substr(0,p1);
            string bas=expr.substr(p1+1,p2-(p1+1));
              // parse offset with base=0
              ins.imm= stoi(off,nullptr,0);
              ins.rs= ParseRegister(bas);
        }
        return;
    }
}

// -------------------------------------------------------------------
// ParseRegister: converts a register name string to an int code
//
// e.g. "$t0" => 8, "$s1" => 17, "$ra" => 31, etc.
// -------------------------------------------------------------------
int SingleCycleMIPS::ParseRegister(string token){
    // remove trailing comma if any
    if(!token.empty() && token.back()==',') token.pop_back();
    // remove leading '$' if present
    if(!token.empty() && token.front()=='$') token.erase(token.begin());

    if(token=="zero") return 0;
    if(token=="at")   return 1;
    if(token=="v0")   return 2;
    if(token=="v1")   return 3;
    if(token=="a0")   return 4;
    if(token=="a1")   return 5;
    if(token=="a2")   return 6;
    if(token=="a3")   return 7;
    if(token=="t0")   return 8;
    if(token=="t1")   return 9;
    if(token=="t2")   return 10;
    if(token=="t3")   return 11;
    if(token=="t4")   return 12;
    if(token=="t5")   return 13;
    if(token=="t6")   return 14;
    if(token=="t7")   return 15;
    if(token=="s0")   return 16;
    if(token=="s1")   return 17;
    if(token=="s2")   return 18;
    if(token=="s3")   return 19;
    if(token=="s4")   return 20;
    if(token=="s5")   return 21;
    if(token=="s6")   return 22;
    if(token=="s7")   return 23;
    if(token=="t8")   return 24;
    if(token=="t9")   return 25;
    if(token=="k0")   return 26;
    if(token=="k1")   return 27;
    if(token=="gp")   return 28;
    if(token=="sp")   return 29;
    if(token=="fp")   return 30;
    if(token=="ra")   return 31;
    return -1; // unrecognized
}

// -------------------------------------------------------------------
// IsHalt: returns true if instruction is "sll $zero,$zero,0"
// -------------------------------------------------------------------
bool SingleCycleMIPS::IsHalt(const Instruction &ins){
    if(ins.opcode=="sll" && ins.rs==0 && ins.rt==0 && ins.rd==0 && ins.imm==0){
        return true;
    }
    return false;
}

// -------------------------------------------------------------------
// DecodeMonitorRegisters: decide which registers to display in columns
// (3), (4), (5) of the "Monitors" line.
//
// - j => no registers
// - beq/bne => show (rs, rt, -)
// - R-type => show (rs, rt, rd)
// - I-type => show (rs, -, rt)
// -------------------------------------------------------------------
void SingleCycleMIPS::DecodeMonitorRegisters(const Instruction &ins,
                                             string &m3, string &m4, string &m5)
{
    auto regName=[&](int r)->string{
        if(r<0) return "-";
        static const char* regNames[] = {
            "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
            "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
            "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
            "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
        };
        if(r>=0 && r<32) return string(regNames[r]);
        return "-";
    };

    // jump => no registers
    if(ins.opcode=="j"){
        m3="-"; m4="-"; m5="-";
        return;
    }
    // beq/bne => (3=rs,4=rt,5="-")
    if(ins.opcode=="beq"||ins.opcode=="bne"){
        m3=regName(ins.rs);
        m4=regName(ins.rt);
        m5="-";
        return;
    }
    // R-type => (3=rs,4=rt,5=rd)
    if(ins.opcode=="add"||ins.opcode=="addu"||ins.opcode=="sub"||ins.opcode=="subu"||
       ins.opcode=="and"||ins.opcode=="or"||ins.opcode=="nor"||ins.opcode=="slt"||
       ins.opcode=="sltu"){
        m3=regName(ins.rs);
        m4=regName(ins.rt);
        m5=regName(ins.rd);
        return;
    }
    // For shift instructions: sll and srl have a different decode format.
    if (ins.opcode == "sll" || ins.opcode == "srl") {
        m3 = regName(ins.rt);  // the register being shifted (source)
        m4 = "-";
        m5 = regName(ins.rd);  // destination register
        return;
    }
    // I-type => (3=rs,4="-",5=rt)
    if(ins.opcode=="addi"||ins.opcode=="addiu"||ins.opcode=="andi"||ins.opcode=="ori"){
        m3 = regName(ins.rs);
        m4 = "-";
        m5 = regName(ins.rt);
        return;
    }
    if(
        ins.opcode=="slti" || ins.opcode=="sltiu"){
         m3 = regName(ins.rs);  // source
         m4 = "-";
         m5 = regName(ins.rt);  // destination
         return;
     }

    // I-type lw,sw => ( 3=rs(base), 4=rt(dataReg), 5="-") //etsi einai sto sample, den kserw giati 
    if(ins.opcode=="lw"||ins.opcode=="sw"){
        m3 = regName(ins.rs);  // base
        m4 = "-";  // data (to load/store)
        m5 = regName(ins.rt);;
        return;
    }

    // fallback => all "-"
    m3="-"; m4="-"; m5="-";
}

// -------------------------------------------------------------------
// PrintCycleInformation
// - The "Registers" portion -> prints PC plus all 32 regs in hex
// - The "Monitors" portion -> prints essential pipeline-like fields
// -------------------------------------------------------------------
void SingleCycleMIPS::PrintCycleInformation(ostream &out, uint32_t oldPC) {

    auto toHexCustom = [&](int32_t val) -> string {
        uint32_t u = (uint32_t)val;
        // If the upper 16 bits are either 0 or 0xFFFF, print only the lower 16 bits.
        if ((u & 0xFFFF0000) == 0 || (u & 0xFFFF0000) == 0xFFFF0000) {
             ostringstream oss;
             oss << hex << uppercase << (u & 0xFFFF);
             string s = oss.str();
             // Remove any leading zeros (if any)
             size_t pos = s.find_first_not_of('0');
             return (pos == string::npos) ? "0" : s.substr(pos);
        } else {
             ostringstream oss;
             oss << hex << uppercase << u;
             string s = oss.str();
             size_t pos = s.find_first_not_of('0');
             return (pos == string::npos) ? "0" : s.substr(pos);
        }
    };

    auto toHexString = [&](int32_t val) -> string {
        // If the upper 16 bits are either all 0's or all 1's, print only the lower 16 bits.
        if (((uint32_t)val & 0xFFFF0000) == 0 || ((uint32_t)val & 0xFFFF0000) == 0xFFFF0000) {
            ostringstream oss;
            oss << hex << uppercase << (val & 0xFFFF);
            return oss.str();
        } else {
            ostringstream oss;
            oss << hex << uppercase << (uint32_t)val;
            return oss.str();
        }
    };

    auto toHexMin = [&](int32_t val) -> string {
        ostringstream oss;
        oss << hex << uppercase << (uint32_t)val;
        string s = oss.str();
        // Remove leading zeros
        size_t pos = s.find_first_not_of('0');
        if (pos == string::npos)
            return "0";
        else
            return s.substr(pos);
    };

    auto toHexReg = [&](int32_t val) -> string {
        // If the number is nonnegative and less than 0x10000, print it minimally.
        // Otherwise, print the full 32-bit hexadecimal.
        if (val >= 0 && (uint32_t)val < 0x10000) {
            ostringstream oss;
            oss << hex << uppercase << val;
            return oss.str();
        } else {
            ostringstream oss;
            oss << hex << uppercase << (uint32_t)val;
            return oss.str();
        }
    };

    auto toHexIType = [&](int32_t val) -> string {
        ostringstream oss;
        // Print as minimal hex using the same rule as toHexString.
        if (((uint32_t)val & 0xFFFF0000) == 0 || ((uint32_t)val & 0xFFFF0000) == 0xFFFF0000)
            oss << hex << uppercase << (val & 0xFFFF);
        else
            oss << hex << uppercase << (uint32_t)val;
        return oss.str();
    };
    
    auto toHexRType = [&](int32_t val) -> string {
        ostringstream oss;
        oss << hex << uppercase << (uint32_t)val;
        return oss.str();
    };
    
    out << "-----Cycle " << dec << cycleCount << "-----\n";

    // 1) Print Register File
    out << "Registers:\n";
    auto R = [&](int i) { return rf.regs[i]; };

    // Print PC first, then all 32 registers in hex
   out << toHexCustom(rf.pc) << "\t";
for (int i = 0; i < 32; i++) {
    out << toHexCustom(rf.regs[i]) << "\t";
}out << "\n\n";

    // 2) Print "Monitors"
    out << "Monitors:\n";

    // (1) previous PC
    out << hex << uppercase << oldPC << "\t";  
    // (2) the instruction text
    out << IR.originalLine << "\t";

    // (3),(4),(5) => registers from decodeMonitorRegisters
    string m3, m4, m5;
    DecodeMonitorRegisters(IR, m3, m4, m5);
    out << m3 << "\t" << m4 << "\t" << m5 << "\t";

    // (6) Immediate
bool isRtype = false, isItype = false;
// Determine if the instruction is R-type or I-type
if (IR.opcode == "add" || IR.opcode == "addu" || IR.opcode == "sub" || IR.opcode == "subu" ||
    IR.opcode == "and" || IR.opcode == "or" || IR.opcode == "nor" || IR.opcode == "slt" ||
    IR.opcode == "sltu" ) {
    isRtype = true;
} else if (IR.opcode == "addi" || IR.opcode == "addiu" || IR.opcode == "andi" || IR.opcode == "ori" ||
           IR.opcode == "slti" || IR.opcode == "sltiu" || IR.opcode == "lw" || IR.opcode == "sw" ||
           IR.opcode == "beq" || IR.opcode == "bne") {
    isItype = true;
}

// Monitor 6: Contents of rd, rs, rt for R-type; rd, rt, - for I-type; -, rs, rt for branches
if (IR.opcode == "sll" || IR.opcode == "srl") {
    out << toHexCustom(rf.regs[IR.rd]) << "\t";   // destination (result) after execution
    out << toHexCustom(rf.regs[IR.rt]) << "\t";       // operand value (from the register being shifted)
    out << "-\t";                                  // no second source operand
} else
if (isRtype) {
    // R-type: rd, rs, rt
    out << toHexCustom(rf.regs[IR.rd]) << "\t";  // Monitor 6: rd
    out << toHexCustom(rf.regs[IR.rs]) << "\t";  // Monitor 7: rs
    out << toHexCustom(rf.regs[IR.rt]) << "\t";  // Monitor 8: rt
} else if (isItype) {
    // For I-type arithmetic instructions, the destination register (rt) is updated.
    // We want to show the updated values from the register file.
    out << toHexCustom(rf.regs[IR.rt]) << "\t";  // Monitor 6: destination (rt)
    out << toHexCustom(rf.regs[IR.rs]) << "\t";  // Monitor 7: source (rs)
    out << "-\t";                                     // Monitor 8: dash

    
    //if (IR.opcode == "sw" || IR.opcode == "lw") {
        // lw/sw: -, rt, rs
        //out << "-\t";                                  // Monitor 6: -
       // out << hex << uppercase << regA << "\t";       // Monitor 7: rs (regA)
       // out << hex << uppercase << regB << "\t";       // Monitor 8: rt (regB)
   // }else {
        // I-type: rd, rt, -
        //out << hex << uppercase << rf.regs[IR.rd] << "\t";  // Monitor 6: rd
        //out << hex << uppercase << regB << "\t";       // Monitor 7: rt (regB)
       // out << "-\t";                                  // Monitor 8: -
   // }
} else {
    // Other cases (e.g., j): -, -, -
    out << "-\t";  // Monitor 6: -
    out << "-\t";  // Monitor 7: -
    out << "-\t";  // Monitor 8: -
}

    // (9) => ALU out
    if (IR.opcode == "sll" || IR.opcode == "srl") {
        out << toHexCustom(aluOut) << "\t";
    } else if (isItype) {
        out << toHexCustom(aluOut) << "\t";
    } else {
        out << toHexCustom(aluOut) << "\t";
    }

    
    // (10) Branch label if used, else '-'
    out << branchLabelMonitor << "\t";

    // (11) Memory address if lw/sw
    if ((IR.opcode=="lw"||IR.opcode=="sw") && memAddress != 0) {
        out << hex << uppercase << memAddress << "\t";
    } else {
        out << "-\t";
    }

    // (12) Store data if sw
    if (didStore) {
        out << hex << uppercase << storeValue << "\t";
    } else {
        out << "-\t";
    }

    // (13) Load data if lw
    if (didLoad) {
        out << hex << uppercase << memDataReg << "\t";
    } else {
        out << "-\t";
    }

    // (14) Control signals in some textual form
    out << (ctrl.RegisterDestination ? "1\t" : "0\t")
        << (ctrl.JumpSignal          ? "1\t" : "0\t")
        << (ctrl.BranchSignal        ? "1\t" : "0\t")
        << (ctrl.MemoryRead          ? "1\t" : "0\t")
        << (ctrl.MemoryToRegister    ? "1\t" : "0\t")
        << GetAlu2Bits(IR.opcode)    << "\t"      // e.g. "10" ,not sure for the correct bits! Check again on next Part
        << (ctrl.MemoryWrite         ? "1\t" : "0\t")
        << (ctrl.ALUSource           ? "1\t" : "0\t")
        << (ctrl.RegisterWrite       ? "1"   : "0")
        << "\n\n";

    // 3) "Memory State" => addresses updated in this cycle
    out << "Memory State:\n";
    
    // Collect addresses in ascending order
    vector<uint32_t> addrs;
    for(auto &kv : mem.data) {
        addrs.push_back(kv.first);
    }
    sort(addrs.begin(), addrs.end());

    if(addrs.empty()) {
        // If no memory used at all, just a blank line
        out << "\n";
    } else {
        // Print the *values* in ascending offset from $gp
        // e.g. 0 => 100, 4 => 1, 8 => 2, 12 => 1000
        uint32_t gpBase = 0x10008000;

        for(auto addr : addrs) {
            // If you only want to show addresses >= gpBase
            if(addr >= gpBase) {
                out << hex << mem.data[addr] <<"\t";
            }
        }
        out << "\n";  // end with blank line
    }
    // Possibly another newline if you want spacing:
    out << "\n";
}

// -------------------------------------------------------------------
// PrintFinalState: logs final CPU and memory contents when the program
// has finished executing (or on user request).
// -------------------------------------------------------------------
void SingleCycleMIPS::PrintFinalState(ostream &out) {
    out << "-----Final State-----\n";
    out << "Registers:\n";

    // Force $zero to 0 for safety:because sometimes its not zero for some reason 
    rf.regs[0] = 0;

    // Print final PC + all registers
    out << hex << uppercase << rf.pc << "\t";
    for (int i = 0; i < 32; i++) {
        out << rf.regs[i] << "\t";
    }
    out << "\n\nMemory State:\n";

    // base = 0x10008000 (the usual gp)
    uint32_t gpBase = 0x10008000; //logo tou sample Instead of printing real addresses, let's gather them as offsets from $gp
    // in ascending offset from $gp
    // 1) Gather addresses from mem.data into a vector
vector<uint32_t> sortedAddrs;
sortedAddrs.reserve(mem.data.size());
for (auto &kv : mem.data) {
    sortedAddrs.push_back(kv.first);
}

// 2) Sort them ascending
sort(sortedAddrs.begin(), sortedAddrs.end());

// 3) Print values in ascending address order
for (auto addr : sortedAddrs) {
    // Now we get them in offset=0, offset=4, offset=8, offset=12, etc.
    // Print the value in hex or decimal as you wish
    out << hex << uppercase << mem.data[addr] << "\t";
}
    out << "\n\nTotal Cycles:\n" << dec << cycleCount << "\n";
}


// -------------------------------------------------------------------
// main: simply creates a SingleCycleMIPS, asks user for cycle input,
// loads instructions, and runs the simulation.
// -------------------------------------------------------------------
int main() {
    SingleCycleMIPS sim;

    cout << "Enter cycles to print (comma-separated, or 'all', or 'last'. e.g. 30,34,last): ";
    string input;
    getline(cin, input);

    vector<int> cyclesToPrint;
    bool includeLast = false;

    // If user typed "all", we store -1 to indicate we print every cycle
    if (input == "all") {
        cyclesToPrint.push_back(-1);
    }
    else {
        // Otherwise, parse comma-separated tokens
        istringstream iss(input);
        string token;
        while (getline(iss, token, ',')) {
            if (token == "last") {
                includeLast = true;
            } else {
                try {
                    cyclesToPrint.push_back(stoi(token));
                } catch (const std::invalid_argument& e) {
                    cerr << "Invalid input: " << token << " is not a number or 'last'." << endl;
                    return 1;
                }
            }
        }
    }

    // Load instructions from file
    sim.LoadAssembly("simple2025.txt");

    // Run the simulation, printing selected cycles and possibly final state
    sim.RunSimulation("simple_output.txt", cyclesToPrint, includeLast);

    return 0;
}
