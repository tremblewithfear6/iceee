#pragma once
#ifndef SCRIPTCORE_H
#define SCRIPTCORE_H

#include <map>
#include <vector>
#include <string>

enum CoreOpCodes
{
	OP_NOP = 0,   //No operation.

	//Named commands.  Correlate exactly to a single instruction by name.  
	OP_END,         //End script.
	OP_GOTO,        //goto <label>
	OP_RESETGOTO,   //reset_goto <label>
	OP_PRINT,       //print <text>
	OP_PRINTVAR,    //printvar <varname>
	OP_PRINTAPPVAR, //printappvar <string>
	OP_PRINTINTARR, //printintarr <string>
	OP_WAIT,        //wait <milliseconds>
	OP_INC,         //inc <varname>
	OP_DEC,         //dec <varname>
	OP_SET,         //set <varname> <value>
	OP_COPYVAR,     //copyvar <varname:src> <varname:dest>
	OP_GETAPPVAR,   //getappvar <string> <varname>
	OP_CALL,        //call <labelname>           jump to a label, adding the current location to the stack
	OP_RETURN,      //return                     return to the last call
	OP_IARRAPPEND,  //iarrappend <intarray> <intstk:value>
	OP_IARRDELETE,  //iarrdelete <intarray> <int:index>
	OP_IARRVALUE,   //iarrvalue <intarray> <int:index> <var:dest>
	OP_IARRCLEAR,   //iarrclear <intarry>
	OP_IARRSIZE,    //iarrsize <intarray> <var:dest>

	OP_QUEUEEVENT,
	OP_EXECQUEUE,

	//Mixed utility/internal commands.
	OP_PUSHVAR,     //Push the integer value of a variable onto the stack
	OP_PUSHINT,     //Push an integer onto the stack.
	OP_PUSHAPPVAR,  //Push an integer onto the stack by performing an application-defined (not script) variable or property name.
	OP_POP,         //Pop an integer from the stack

	//Strictly internal operations, not accessed through named commands.
	OP_CMP,         //Perform a comparison between the topmost stack values.
	OP_JMP,         //Unconditional jump to an instruction.  Used internally for certain loop control.

	OP_MAXCOREOPCODE
};

enum OpCodeParamType
{
	OPT_NONE = 0,
	OPT_LABEL,    //Operand is a string containing a label name
	OPT_STR,      //Operand is a generic script-defined string
	OPT_INT,      //Operand is an integer
	OPT_VAR,      //Operand is a string containing a variable name
	OPT_APPINT,   //An application defined property or variable name (outside the script) which will be fetched and pushed onto the stack as an integer.
	OPT_INTSTK,   //An integer value that is resolved from different source types (OPT_INT, OPT_VAR, OPT_APPINT) and pushed onto the stack as an integer.  Implementation of commands which use this parameter type are responsible for popping the values off the stack.
	OPT_INTARR    //Operand is a string containing the name of an integer array.
};

enum ComparisonType
{
	CMP_INV = 0,  //Invalid comparison type
	CMP_EQ,       //Equal
	CMP_NEQ,      //Not equal
	CMP_LT,       //Less than
	CMP_LTE,      //Less than or or equal
	CMP_GT,       //Greater than
	CMP_GTE       //Greater than or equal
};

struct OpCodeInfo
{
	const char *name;
	unsigned char opCode;
	unsigned char numParams;
	int paramType[3];
};

namespace ScriptCore
{

extern const int maxCoreOpCode;
extern OpCodeInfo defCoreOpCode[];

struct OpData
{
	unsigned char opCode;
	int param1;
	int param2;
	int param3;
	OpData();
	OpData(int op, int p1, int p2);
	OpData(int op, int p1, int p2, int p3);
};

struct LabelDef
{
	std::string name;
	int instrOffset;
	LabelDef(const char *labelName, int targInst);
};

struct IntArray
{
	std::string name;
	std::vector<int> arrayData;

	IntArray();
	IntArray(const char *intArrName);

	void Clear(void);
	int Size(void);
	void Append(int value);
	void RemoveByIndex(int index);
	int GetValueByIndex(int index);
	int GetIndexByValue(int value);
	bool VerifyIndex(int index);
	void DebugPrintContents(void);
	
	static const int MAX_ARRAY_DATA_SIZE = 64;
};

//Holds a triggered event.  If it is ready to fire, the label name will be called.
class ScriptEvent
{
public:
	std::string mLabel;              //Label to jump to.
	unsigned long mFireTime;         //Time to fire this event.
	ScriptEvent(const char *label, unsigned long fireTime);
};

// Used by the ScriptCompiler class to store certain line information.
struct BlockData
{
	int mLineNumber;  //Line number in the source file of this block
	int mInstIndex;     //Instruction index of the first comparing instruction.
	int mInstIndexElse; //Instruction index of the second block if an "else" statement is encountered.
	int mNestLevel;   //Nest level;
	bool mResolved;   //Must be true when compilation has finished, otherwise an error might've occurred.
	bool mUseElse;
	BlockData();
};

// Manages and stores all temporary helper data when a script is being compiled.  Can be safely
// discarded after compilation is finished.
class ScriptCompiler
{
public:

	int mCurrentNestLevel;
	int mLastNestLevel;
	std::vector<BlockData> mBlockData;
	const char *mSourceFile;
	int mLineNumber;
	std::vector<std::string> mTokens;
	int mInlineBeginInstr;
	std::map<std::string, std::string> mSymbols;  //Maps symbol names to their values.  Tokens that match the symbols will be replaced with their values before they are compiled into the instructions.
	std::vector<int> mPendingLabelReference;

	ScriptCompiler();
	void OpenBlock(int lineNumber, int instructionIndex);
	bool CloseBlock(void);
	BlockData* GetLastUnresolvedBlock(void);
	void AddSymbol(const std::string &key, const std::string &value);
	bool HasSymbol(const std::string &token);
	void CheckSymbolReplacements(void);
	bool ExpectTokens(size_t count, const char *op, const char *desc);
	void AddPendingLabelReference(int instructionIndex);
};

class ScriptDef
{
	friend class ScriptPlayer;

public:
	typedef std::vector<std::string> STRINGLIST;
	std::string scriptName;            //Internal name of the script.
	std::vector<OpData> instr;         //Compiled array of instructions.
	STRINGLIST stringList;             //Holds all registered strings.

	ScriptDef();
	void ClearBase(void);             //Initialize all data to their reset state.  It will call ClearDerived()
	virtual void ClearDerived(void);  //Override this to allow clearing of new members present in a derived class.
	void CompileFromSource(const char *sourceFile);
	void CompileLine(char *line, ScriptCompiler &compileData);
	void BeginInlineBlock(ScriptCompiler &compileData);       //Begins a block of inline compiled code, essentially resuming compilation elsewhere.  Existing labels, variables, etc are accessible as normal.
	void FinishInlineBlock(ScriptCompiler &compileData);      //Closes a block of inline compiled code, running label resolution on them.
	void OutputDisassemblyToFile(FILE *output);

protected:
	std::vector<LabelDef> label;             //Holds all registered labels.
	std::vector<std::string> varName;        //Holds all registered variable names.
	int curInst;                             //Tracks the current instruction as opcodes are generated.
	std::vector<std::string> extVarName;     //Holds extended variable names.
	std::map<std::string, int> mLabelMap;    //Maps label names to their array index for fast lookups.
	std::vector<IntArray> mIntArray;         //An array of integer arrays.

	//General string utilities.
	void Tokenize(const char *srcString, STRINGLIST &destList);
	const char * GetSubstring(STRINGLIST &strList, int index);
	const char * GetOffsetIntoString(const char *value, int offset);

	void FinalizeCompile(ScriptCompiler &compileData);  //Finalize the opcode data, set jump targets, report bad labels, etc.
	int ResolveOperand(int paramType, const char *token, int &pushType);
	int CreateLabel(const char *name, int targInst);
	int GetLabelIndex(const char *name);
	int CreateString(const char *name);
	int GetStringIndex(const char *name);
	int CreateVariable(const char *name);
	int GetVariableIndex(const char *name);
	int CreateIntArray(const char *name);
	int GetIntArrayIndex(const char *name);
	void PushOpCode(int opcode, int param1, int param2);
	void PushOpCode(int opcode, int param1, int param2, int param3);
	int ResolveOperandType(const char *token, int &value);
	int ResolveComparisonType(const char *token);
	int ConvertParamTypeToPushType(int paramType);
	void SetScriptSpeed(const char *token);
	bool CanIdle(void);
	bool UseEventQueue(void);
	void SetFlag(unsigned int flag, unsigned int value);
	bool HasFlag(unsigned int flag);

	void SetMetaDataBase(const char *opname, ScriptCompiler &compileData);
	OpCodeInfo* GetInstructionData(int opcode);
	OpCodeInfo* GetInstructionDataByName(const char *name);

	//These functions MUST be overridden by derived classes so they can
	//access their custom extended instruction lists.
	virtual void GetExtendedOpCodeTable(OpCodeInfo **arrayStart, size_t &arraySize);

	//Optional overrides for handling additional commands.
	virtual void SetMetaDataDerived(const char *opname, ScriptCompiler &compileData);
	virtual bool HandleAdvancedCommand(const char *commandToken, ScriptCompiler &compileData);

private:
	int scriptSpeed;
	int scriptIdleSpeed;
	bool queueExternalJumps;
	int queueCallStyle;
	unsigned int mFlags;

	static const int DEFAULT_INSTRUCTIONS_PER_CYCLE = 1;
	static const int DEFAULT_INSTRUCTIONS_PER_IDLE_CYCLE = 0;

	enum Flags
	{
		FLAG_NONE              = 0,
		FLAG_REPORT_END        = 1,
		FLAG_REPORT_LABEL      = 2,
		FLAG_REPORT_ALL        = 0x0FFFFFFF,
		FLAG_BITS              = 0xFFFFFFFF,

		FLAG_DEFAULT           = 0
	};

	enum CallStyle
	{
		CALLSTYLE_CALL = 0,
		CALLSTYLE_GOTO = 1
	};

	bool Expect(const char *token, int paramType);
	const char* GetExpectedDetail(int paramType);
	void IncludeFile(const std::string &token, ScriptCompiler &compileData);
};

class ScriptPlayer
{
public:
	ScriptDef *def;                      //Pointer to the script definition that this player is executing.
	int curInst;                         //Index of the current instruction.
	bool active;                         //If true, the script is considered to be running (has not terminated).
	unsigned long nextFire;              //Time when the next instruction can run.  Used for waits.

	ScriptPlayer();
	~ScriptPlayer();
	std::vector<int> vars;
	std::vector<IntArray> intArray;      //An array of integer arrays.
	std::vector<ScriptEvent> scriptEventQueue;

	void Initialize(ScriptDef *defPtr);
	bool RunSingleInstruction(void);     //Run a single instruction.
	virtual void RunImplementationCommands(int opcode);
	void RunUntilWait(void);
	void RunAtSpeed(int maxCommands);
	void RunScript(void);                //Run the script until it ends.
	bool JumpToLabel(const char *name);  //Immediately jump script execution to the beginning of a specific label.
	void FullReset(void);
	bool IsWaiting(void);
	bool CanRunIdle(void);
	void HaltExecution(void);

protected:
	static const size_t MAX_STACK_SIZE = 16;
	static const size_t MAX_QUEUE_SIZE = 16;
	std::vector<int> varStack;

	virtual int GetApplicationPropertyAsInteger(const char *propertyName);  //Override this function to substitute application-defined variables.

	void PushVarStack(int value);
	int PopVarStack(void);
	const char * GetStringPtr(int index);
	int GetVarValue(int index);

	std::vector<int> callStack;
	void PushCallStack(int value);
	int PopCallStack(void);
	void Call(int targetInstructionIndex);
	void ResetGoto(int targetInstructionIndex);

	void QueueEvent(const char *labelName, unsigned long fireDelay);
	bool ExecQueue(void);
	bool PerformJumpRequest(const char *name, int callStyle);

	const char *GetStringTableEntry(int index);
	int VerifyIntArrayIndex(int index);

	void SetVar(unsigned int index, int value);
	short advance;   //Number of instructions to advance after the current instruction.  Used for retry commands or jump elsewhere.
};

void PrintMessage(const char *format, ...);

//namespace ScriptCore
}

#endif //#define SCRIPTCORE_H
