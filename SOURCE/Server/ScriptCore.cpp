#include "ScriptCore.h"
#include "FileReader.h"
#include <stdarg.h>
#include <string.h>
#include "StringList.h"

extern unsigned long g_ServerTime;

namespace ScriptCore
{
OpCodeInfo defCoreOpCode[] = {
	{ "nop",          OP_NOP,          0, {OPT_NONE,   OPT_NONE,   OPT_NONE  }},
	{ "end",          OP_END,          0, {OPT_NONE,   OPT_NONE,   OPT_NONE  }},
	{ "goto",         OP_GOTO,         1, {OPT_LABEL,  OPT_NONE,   OPT_NONE  }},
	{ "reset_goto",   OP_RESETGOTO,    1, {OPT_LABEL,  OPT_NONE,   OPT_NONE  }},
	{ "print",        OP_PRINT,        1, {OPT_STR,    OPT_NONE,   OPT_NONE  }},
	{ "printvar",     OP_PRINTVAR,     1, {OPT_VAR,    OPT_NONE,   OPT_NONE  }},
	{ "printappvar",  OP_PRINTAPPVAR,  1, {OPT_STR,    OPT_NONE,   OPT_NONE  }},
	{ "printintarr",  OP_PRINTINTARR,  1, {OPT_INTARR, OPT_NONE,   OPT_NONE  }},
	{ "wait",         OP_WAIT,         1, {OPT_INT,    OPT_NONE,   OPT_NONE  }},
	{ "inc",          OP_INC,          1, {OPT_VAR,    OPT_NONE,   OPT_NONE  }},
	{ "dec",          OP_DEC,          1, {OPT_VAR,    OPT_NONE,   OPT_NONE  }},
	{ "set",          OP_SET,          2, {OPT_VAR,    OPT_INT,    OPT_NONE  }},
	{ "copyvar",      OP_COPYVAR,      2, {OPT_VAR,    OPT_VAR,    OPT_NONE  }},
	{ "getappvar",    OP_GETAPPVAR,    2, {OPT_STR,    OPT_VAR,    OPT_NONE  }},
	{ "call",         OP_CALL,         1, {OPT_LABEL,  OPT_NONE,   OPT_NONE  }},
	{ "return",       OP_RETURN,       0, {OPT_NONE,   OPT_NONE,   OPT_NONE  }},
	{ "iarrappend",   OP_IARRAPPEND,   2, {OPT_INTARR, OPT_INTSTK, OPT_NONE  }},
	{ "iarrdelete",   OP_IARRDELETE,   2, {OPT_INTARR, OPT_INTSTK, OPT_NONE  }},
	{ "iarrvalue",    OP_IARRVALUE,    3, {OPT_INTARR, OPT_INTSTK, OPT_VAR   }},
	{ "iarrclear",    OP_IARRCLEAR,    1, {OPT_INTARR, OPT_NONE,   OPT_NONE  }},
	{ "iarrsize",     OP_IARRSIZE,     2, {OPT_INTARR, OPT_VAR,    OPT_NONE  }},
	{ "queue_event",  OP_QUEUEEVENT,   2, {OPT_STR,    OPT_INT,    OPT_NONE  }},
	{ "exec_queue",   OP_EXECQUEUE,    0, {OPT_NONE,   OPT_NONE,   OPT_NONE  }},

	//These functions are used internally, but can be called directly in the script as well.
	{ "pushvar",      OP_PUSHVAR,      1, {OPT_VAR,    OPT_NONE,   OPT_NONE  }},
	{ "pushint",      OP_PUSHINT,      1, {OPT_INT,    OPT_NONE,   OPT_NONE  }},
	{ "pop",          OP_POP,          1, {OPT_VAR,    OPT_NONE,   OPT_NONE  }},

	//These functions are for internal purpose only, and should not be used by a script.
	{ "_pushappvar",  OP_PUSHAPPVAR,   0, {OPT_NONE,   OPT_NONE,   OPT_NONE  }},
	{ "_cmp",         OP_CMP,          0, {OPT_NONE,   OPT_NONE,   OPT_NONE  }},
	{ "_jmp",         OP_JMP,          0, {OPT_NONE,   OPT_NONE,   OPT_NONE  }},
};

OpCodeInfo extCoreOpCode[] = {
	{ "nop",   OP_NOP,   0, {OPT_NONE,   OPT_NONE }},
};

const int maxCoreOpCode = sizeof(defCoreOpCode) / sizeof(OpCodeInfo);
const int maxExtOpCode = sizeof(extCoreOpCode) / sizeof(OpCodeInfo);

OpData:: OpData()
{
	opCode = OP_NOP;
	param1 = 0;
	param2 = 0;
	param3 = 0;
}

OpData :: OpData(int op, int p1, int p2)
{ 
	opCode = op;
	param1 = p1;
	param2 = p2;
	param3 = 0;
}

OpData :: OpData(int op, int p1, int p2, int p3)
{
	opCode = op;
	param1 = p1;
	param2 = p2;
	param3 = p3;
}

LabelDef :: LabelDef(const char *labelName, int targInst)
{
	name = labelName;
	instrOffset = targInst;
}


ScriptDef :: ScriptDef()
{
	curInst = 0;
	scriptSpeed = DEFAULT_INSTRUCTIONS_PER_CYCLE;
	scriptIdleSpeed = DEFAULT_INSTRUCTIONS_PER_IDLE_CYCLE;
	queueExternalJumps = false;
	queueCallStyle = CALLSTYLE_CALL;
	mFlags = FLAG_DEFAULT;
}

void ScriptDef :: ClearBase(void)
{
	scriptName.clear();
	instr.clear();
	stringList.clear();

	label.clear();
	varName.clear();
	curInst = 0;
	extVarName.clear();
	mLabelMap.clear();
	mIntArray.clear();

	scriptSpeed = DEFAULT_INSTRUCTIONS_PER_CYCLE;
	scriptIdleSpeed = DEFAULT_INSTRUCTIONS_PER_IDLE_CYCLE;
	queueExternalJumps = false;
	queueCallStyle = CALLSTYLE_CALL;
	mFlags = FLAG_DEFAULT;

	ClearDerived();
}

// Stub function, if additional members are defined in a derived class, then override this function
// to clear the new members.
void ScriptDef :: ClearDerived(void)
{
}

void ScriptDef :: CompileFromSource(const char *sourceFile)
{
	FileReader lfr;
	if(lfr.OpenText(sourceFile) != Err_OK)
	{
		PrintMessage("[WARNING] InstanceScript::CompileFromSource() unable to open file: %s", sourceFile);
		return;
	}

	lfr.CommentStyle = Comment_Semi;

	ScriptCompiler compileData;
	compileData.mSourceFile = sourceFile;

	while(lfr.FileOpen() == true)
	{
		lfr.ReadLine();
		compileData.mLineNumber = lfr.LineNumber;
		CompileLine(lfr.DataBuffer, compileData);
	}
	FinalizeCompile(compileData);
}

void ScriptDef :: CompileLine(char *line, ScriptCompiler &compileData)
{
	STRINGLIST &tokens = compileData.mTokens;  //Alias so we don't have to change the rest of the code.

	Tokenize(line, tokens);
	compileData.CheckSymbolReplacements();
	const char *opname = GetSubstring(tokens, 0);
	if(opname != NULL)
	{
		if(opname[0] == ':')
		{
			CreateLabel(GetOffsetIntoString(opname, 1), curInst);
		}
		else if(strcmp(opname, "if") == 0)
		{
			if(tokens.size() < 4)
				PrintMessage("Syntax error: not enough operands for IF statement in [%s line %d]", compileData.mSourceFile, compileData.mLineNumber);
			else
			{
				int vleft = 0;
				int vright = 0;
				int left = ResolveOperandType(tokens[1].c_str(), vleft);
				int cmp = ResolveComparisonType(tokens[2].c_str());
				int right = ResolveOperandType(tokens[3].c_str(), vright);
				if(cmp == CMP_INV)
					PrintMessage("Invalid comparison operator: [%s] in [%s line %d]", compileData.mSourceFile, compileData.mLineNumber);
				else
				{
					bool valid = true;
					//Push the values onto the stack backwards, so that when
					//popped off they'll follow a left to right order.
					switch(right)
					{
					case OPT_VAR: PushOpCode(OP_PUSHVAR, vright, 0); break;
					case OPT_INT: PushOpCode(OP_PUSHINT, vright, 0); break;
					case OPT_APPINT: PushOpCode(OP_PUSHAPPVAR, vright, 0); break;
					default: PrintMessage("Invalid operator [%s] for IF statement [%s line %d]", tokens[3].c_str(), compileData.mSourceFile, compileData.mLineNumber); valid = false; break;
					}
		
					switch(left)
					{
					case OPT_VAR: PushOpCode(OP_PUSHVAR, vleft, 0); break;
					case OPT_INT: PushOpCode(OP_PUSHINT, vleft, 0); break;
					case OPT_APPINT: PushOpCode(OP_PUSHAPPVAR, vleft, 0); break;
					default: PrintMessage("Invalid operator [%s] for IF statement [%s line %d]", tokens[1].c_str(), compileData.mSourceFile, compileData.mLineNumber); valid = false; break;
					}

					if(valid == true)
					{
						// Normally the IF command will continue to the next line
						// if it succeeds.  If it fails, jump over the next immediate
						// line by 2 spaces, unless otherwise programmed.
						// Line 1 = if a != b
						// Line 2 =   print "Not equal"
						// Line 3 = wait 1000
						int jumpCount = 2;
						if(tokens.size() >= 6)
							if(tokens[5].compare("else") == 0)
								jumpCount = atoi(tokens[5].c_str());

						compileData.OpenBlock(compileData.mLineNumber, curInst);
						PushOpCode(OP_CMP, cmp, jumpCount);
					}
				}
			}
		}
		else if(strcmp(opname, "endif") == 0)
		{
			BlockData *block = compileData.GetLastUnresolvedBlock();
			if(block == NULL)
				PrintMessage("[WARNING] ENDIF without a matching IF (%s line %d)", compileData.mSourceFile, compileData.mLineNumber);
			else
			{
				//In both cases, we want to alter the CMP instruction data with the jump offset
				//to skip over the 'true' statement block when the result is 'false'
				if(block->mUseElse == false)  //IF...ENDIF format
				{
					instr[block->mInstIndex].param2 = curInst - block->mInstIndex;
				}
				else  //IF...ELSE...ENDIF format
				{
					// +1 to accomodate the added JMP instruction.  mInstIndexElse points to the first
					// instruction of the 'false' block.
					instr[block->mInstIndex].param2 = block->mInstIndexElse - block->mInstIndex + 1;

					//For "else" blocks, we need to modify the JMP instruction (the last statement
					//in the 'true' block) to jump over the ELSE block.
					instr[block->mInstIndexElse].param1 = curInst;
				}
				compileData.CloseBlock();
			}
		}
		else if(strcmp(opname, "else") == 0)
		{
			BlockData *block = compileData.GetLastUnresolvedBlock();
			if(block == NULL)
				PrintMessage("[WARNING] ELSE without a matching IF (%s line %d)", compileData.mSourceFile, compileData.mLineNumber);
			else
			{
				block->mInstIndexElse = curInst;
				block->mUseElse = true;

				//The instructions executed in a 'true' condition will be followed by a jump
				//to bypass the instructions executed in a 'false' condition.
				//The instruction index will be adjusted when "endif" is encountered and processed,
				//for now it will default to fall through into the next instruction. 
				PushOpCode(OP_JMP, curInst + 1, 0);  
			}
		}
		else if(strcmp(opname, "recompare") == 0)
		{
			BlockData *block = compileData.GetLastUnresolvedBlock();
			if(block == NULL)
				PrintMessage("[WARNING] RECOMPARE without a matching IF (%s line %d)", compileData.mSourceFile, compileData.mLineNumber);
			else
			{
				//Jump to the CMP instruction of the "if" statement, plus the two PUSH operations
				//that come before. 
				PushOpCode(OP_JMP, block->mInstIndex - 2, 0);
			}
		}
		else if(opname[0] == '#')
		{
			SetMetaDataBase(opname, compileData);
		}
		else if(HandleAdvancedCommand(opname, compileData) == true)
		{
			//Do nothing, the function will take care of it.
		}
		else
		{
			OpCodeInfo *opinfo = GetInstructionDataByName(opname);
			if(opinfo->opCode == OP_NOP)
			{
				PrintMessage("Unknown instruction [%s] [%s line %d]", opname, compileData.mSourceFile, compileData.mLineNumber);
			}
			else
			{
				int param[3] = {0};
				bool resolveLabels = false;
				for(int i = 0; i < opinfo->numParams; i++)
				{
					const char *paramToken = GetSubstring(tokens, i + 1);
					int pushType = OPT_NONE;
					/*
					if(Expect(paramToken, opinfo->paramType[i]) == false)
						PrintMessage("Token [%s] does not match the expected type [%s] on [%s line %d]", paramToken, GetExpectedDetail(opinfo->paramType[i]), compileData.mSourceFile, compileData.mLineNumber);
					*/
					param[i] = ResolveOperand(opinfo->paramType[i], paramToken, pushType);

					if(opinfo->paramType[i] == OPT_LABEL)
						resolveLabels = true;

					// If this isn't an explicit argument type (like OPT_INT, OPT_VAR, etc), it means
					// that multiple sources may provide data via a stack PUSH operation, and won't be
					// embedded into the compiled instruction data.  This is similar to how the "if"
					// statement works, which allows and permutation of comparisions between 
					// integers and variables.
					// The associated opcode command must be responsible for popping the values off the
					// stack.
					if(pushType != OPT_NONE)
					{
						pushType = ConvertParamTypeToPushType(pushType);
						if(pushType == OPT_NONE)
						{
							PrintMessage("Could not convert parameter token [%s] to expected type [%s line %d]", paramToken, compileData.mSourceFile, compileData.mLineNumber);
						}
						else
						{
							PushOpCode(pushType, param[i], 0);
							param[i] = 0;  //Set to zero since the instruction will be using stack POP data instead.
						}
					}
				}

				//This appends a list of instruction indexes which need to have their label data resolved.
				//Add here before the current instruction index is incremented.
				if(resolveLabels == true)
					compileData.AddPendingLabelReference(curInst);

				PushOpCode(opinfo->opCode, param[0], param[1], param[2]);
			}
		}
	}
}

void ScriptDef :: BeginInlineBlock(ScriptCompiler &compileData)
{
	compileData.mSourceFile = "<inline>";
	compileData.mInlineBeginInstr = curInst;
}

void ScriptDef :: FinishInlineBlock(ScriptCompiler &compileData)
{
	FinalizeCompile(compileData);
}

void ScriptDef :: FinalizeCompile(ScriptCompiler &compileData)
{
	//Resolve labels and jumps
	for(size_t i = 0; i < label.size(); i++)
	{
		if(label[i].instrOffset == -1)
		{
			PrintMessage("Unresolved label: %s", label[i].name.c_str());
			label[i].instrOffset = 0;
		}
	}

	for(size_t i = 0; i < compileData.mPendingLabelReference.size(); i++)
	{
		int index = compileData.mPendingLabelReference[i];
		if(index >= 0)
		{
			OpCodeInfo *opInfo = GetInstructionData(instr[index].opCode);
			if(opInfo != NULL)
			{
				if(opInfo->paramType[0] == OPT_LABEL)
					instr[index].param1 = label[instr[index].param1].instrOffset;
				if(opInfo->paramType[1] == OPT_LABEL)
					instr[index].param2 = label[instr[index].param2].instrOffset;
				if(opInfo->paramType[2] == OPT_LABEL)
					instr[index].param3 = label[instr[index].param3].instrOffset;
			}
		}
	}
	compileData.mPendingLabelReference.clear();

	/*  OBSOLETE: needed a new system because extended opcode tables which used label resolution
	would not be resolved here and cause bugs.

	for(size_t i = startInstruction; i < instr.size(); i++)
	{
		//Replace the jump target from a label index to an instruction offet
		if(instr[i].opCode == OP_GOTO || instr[i].opCode == OP_CALL)
			instr[i].param1 = label[instr[i].param1].instrOffset;
	}
	*/
}

//Try to verify whether the token string matches the expected type.
bool ScriptDef :: Expect(const char *token, int paramType)
{
	switch(paramType)
	{
	case OPT_NONE: return false;
	case OPT_LABEL: return (GetLabelIndex(token) >= 0);
	case OPT_STR: return true;
	case OPT_INT: return true;
	case OPT_VAR: return (GetVariableIndex(token) >= 0);
	case OPT_APPINT: return true;
	case OPT_INTSTK: return true;
	case OPT_INTARR: return (GetIntArrayIndex(token) >= 0);
	default: return true;
	}
	return true;
}


const char* ScriptDef :: GetExpectedDetail(int paramType)
{
	switch(paramType)
	{
	case OPT_NONE: return "<none>";
	case OPT_LABEL: return "label";
	case OPT_STR: return "string";
	case OPT_INT: return "integer";
	case OPT_VAR: return "variable";
	case OPT_APPINT: return "property name";
	case OPT_INTSTK: return "resolvable integer result";
	case OPT_INTARR: return "integer array name";
	}
	return "<unknown>";
}

int ScriptDef :: ResolveOperand(int paramType, const char *token, int &pushType)
{
	pushType = OPT_NONE;

	if(token == NULL)
		return 0;

	switch(paramType)
	{
	case OPT_LABEL:
		return CreateLabel(token, -1);
	case OPT_STR:
		return CreateString(token);
	case OPT_INT:
		return atoi(token);
	case OPT_VAR:
		return CreateVariable(token);
	case OPT_INTARR:
		return CreateIntArray(token);
	case OPT_INTSTK:
		{
			int value = 0;
			pushType = ResolveOperandType(token, value);
			return value;
		}
		break;
	}
	return 0;
}

// Some command instructions accept parameters that may be resolved from multiple types, with the result
// placed on the stack.  This converts the parameter type to the associated push opcode that must
// be performed.
int ScriptDef :: ConvertParamTypeToPushType(int paramType)
{
	switch(paramType)
	{
	case OPT_INT:
		return OP_PUSHINT;
	case OPT_VAR:
		return OP_PUSHVAR;
	case OPT_APPINT: return OP_PUSHAPPVAR;
	}
	return OPT_NONE;
}

int ScriptDef :: CreateLabel(const char *name, int targInst)
{
	if(name == NULL)
	{
		PrintMessage("Label name is null."); 
		return -1;
	}
	if(name[0] == 0)
	{
		PrintMessage("Label name cannot be empty."); 
		return -1;
	}

	int r = GetLabelIndex(name);
	if(r == -1)
	{
		label.push_back(LabelDef(name, targInst));
		r = label.size() - 1;
		mLabelMap[name] = r;
	}
	if(targInst >= 0)
		label[r].instrOffset = targInst;
	return r;
}

int ScriptDef :: GetLabelIndex(const char *name)
{
	std::map<std::string, int>::iterator it;
	it = mLabelMap.find(name);
	if(it == mLabelMap.end())
		return -1;
	
	return it->second;

	/*
	for(size_t i = 0; i < label.size(); i++)
		if(label[i].name.compare(name) == 0)
			return i;
	return -1;
	*/
}

int ScriptDef :: CreateString(const char *name)
{
	if(name == NULL)
	{
		PrintMessage("String cannot be NULL."); 
		return -1;
	}

	int r = GetStringIndex(name);
	if(r == -1)
	{
		stringList.push_back(name);
		r = stringList.size() - 1;
	}
	return r;
}

int ScriptDef :: GetStringIndex(const char *name)
{
	for(size_t i = 0; i < stringList.size(); i++)
		if(stringList[i].compare(name) == 0)
			return i;
	return -1;
}

int ScriptDef :: CreateVariable(const char *name)
{
	for(size_t i = 0; i < varName.size(); i++)
		if(varName[i].compare(name) == 0)
			return i;
	varName.push_back(name);
	return varName.size() - 1;
}

int ScriptDef :: GetVariableIndex(const char *name)
{
	for(size_t i = 0; i < varName.size(); i++)
		if(varName[i].compare(name) == 0)
			return i;
	return -1;
}

int ScriptDef :: CreateIntArray(const char *name)
{
	if(GetIntArrayIndex(name) == -1)
	{
		mIntArray.push_back(IntArray(name));
	}
	return (int)mIntArray.size() - 1;
}

int ScriptDef :: GetIntArrayIndex(const char *name)
{
	for(size_t i = 0; i < mIntArray.size(); i++)
		if(mIntArray[i].name.compare(name) == 0)
			return (int)i;
	return -1;
}

void ScriptDef :: OutputDisassemblyToFile(FILE *output)
{
	if(output == NULL)
		output = stdout;

	fprintf(output, "Script: %s\r\n", scriptName.c_str());
	fprintf(output, "Labels: %lu\r\n", label.size());
	for(size_t i = 0; i < label.size(); i++)
		fprintf(output, "[%lu] = %s : %d\r\n", i, label[i].name.c_str(), label[i].instrOffset);

	fprintf(output, "\r\nStrings:\r\n");
	for(size_t i = 0; i < stringList.size(); i++)
		fprintf(output, "[%lu]=[%s]\r\n", i, stringList[i].c_str());

	fprintf(output, "\r\nVariables:\r\n");
	for(size_t i = 0; i < varName.size(); i++)
		fprintf(output, "[%lu]=[%s]\r\n", i, varName[i].c_str());

	fprintf(output, "\r\nInstructions:\r\n");
	for(size_t i = 0; i < instr.size(); i++)
	{
		OpCodeInfo *opi = GetInstructionData(instr[i].opCode);
		fprintf(output, "[%lu] : %s %d %d\r\n", i, opi->name, instr[i].param1, instr[i].param2);
	}
	fprintf(output, "\r\n");
}

void ScriptDef :: Tokenize(const char *srcString, STRINGLIST &destList)
{
	std::string subStr;
	destList.clear();
	//printf(" 12345678901234567890\n");
	//printf("[%s]\n", srcBuf);
	int len = strlen(srcString);
	int start = -1;
	int end = -1;
	bool quote = false;
	for(int i = 0; i <= len; i++)
	{
		switch(srcString[i])
		{
		case '\t':
		case ' ': //Terminate a word if it has started, and is not within a quoted phrase.
			if(start != -1)
				if(quote == false)
					end = i - 1;
			break;
		case '"': //Begin or terminate a quote phrase.
			//printf("Pos: %d\n", i);
			if(quote == false)
			{
				quote = true;
				start = i;
			}
			else
				end = i;
			break;
		case '\0': //Terminate a word if it has started, and abort a quote if it hasn't completed.
			if(quote == false)
			{
				if(start != -1)
					end = i - 1;
			}
			else
			{
				//printf("aborted\n");
				start = -1;
				end = -1;
				quote = false;
			}
			break;
		default:
			if(start == -1)
				start = i;
			break;
		}
		if(end != -1)
		{
			if(quote == true)
			{
				//Drop the quotation marks from both ends.
				start++;
				end--;
			}
			if(end + 1 <= len)
			{
				subStr.assign(&srcString[start], end - start + 1);
				destList.push_back(subStr);
				if(quote == true)
				{
					stringList.push_back(subStr);
					quote = false;
				}
				/*
				char temp = srcString[end];
				srcString[end + 1] = 0;
				destList.push_back(&srcString[start]);
				if(quote == true)
				{
					stringList.push_back(&srcString[start]);
					quote = false;
				}

				srcString[end + 1] = temp;
				*/
			}
			start = -1;
			end = -1;
		}
	}

	/*
	printf("Tokenize: %d :", resList.size());
	for(size_t i = 0; i < resList.size(); i++)
		printf(" [%s]", resList[i].c_str());
	printf("\n\n\n");
	*/
}

const char * ScriptDef :: GetSubstring(STRINGLIST &strList, int index)
{
	if(index < 0 || index >= (int)strList.size())
		return NULL;
	return strList[index].c_str();
}

const char * ScriptDef :: GetOffsetIntoString(const char *value, int offset)
{
	if(offset < 0 || offset >= (int)strlen(value))
		return NULL;
	return &value[offset];
}

void ScriptDef :: PushOpCode(int opcode, int param1, int param2)
{
	instr.push_back(OpData(opcode, param1, param2));
	curInst++;
}

void ScriptDef :: PushOpCode(int opcode, int param1, int param2, int param3)
{
	instr.push_back(OpData(opcode, param1, param2, param3));
	curInst++;
}

OpCodeInfo* ScriptDef :: GetInstructionData(int opcode)
{
	for(int i = 0; i < maxCoreOpCode; i++)
		if(defCoreOpCode[i].opCode == opcode)
			return &defCoreOpCode[i];

	//Fetch the table, which may be different for derived classes.
	OpCodeInfo *arrayStart = NULL;
	size_t arraySize = 0;
	GetExtendedOpCodeTable(&arrayStart, arraySize);

	/*
	for(int i = 0; i < maxExtOpCode; i++)
		if(extCoreOpCode[i].opCode == opcode)
			return &extCoreOpCode[i];
	*/
	for(size_t i = 0; i < arraySize; i++)
	{
		if(arrayStart[i].opCode == opcode)
			return &arrayStart[i];
	}

	PrintMessage("Unidentified opcode: %d", opcode);
	return &defCoreOpCode[0];
}

OpCodeInfo* ScriptDef :: GetInstructionDataByName(const char *name)
{
	for(int i = 0; i < maxCoreOpCode; i++)
		if(strcmp(defCoreOpCode[i].name, name) == 0)
			return &defCoreOpCode[i];

	OpCodeInfo *arrayStart = NULL;
	size_t arraySize = 0;
	GetExtendedOpCodeTable(&arrayStart, arraySize);

	/*
	for(int i = 0; i < maxExtOpCode; i++)
		if(strcmp(extCoreOpCode[i].name, name) == 0)
			return &extCoreOpCode[i];
	*/
	for(size_t i = 0; i < arraySize; i++)
		if(strcmp(arrayStart[i].name, name) == 0)
			return &arrayStart[i];

	return &defCoreOpCode[0];
}

// Override this to return the pointer to the first element of the opcode definition array that the
// derived class should use, and its array size.
void ScriptDef :: GetExtendedOpCodeTable(OpCodeInfo **arrayStart, size_t &arraySize)
{
	*arrayStart = ScriptCore::extCoreOpCode;
	arraySize = ScriptCore::maxExtOpCode;
}

int ScriptDef :: ResolveOperandType(const char *token, int &value)
{
	if(token == NULL)
	{
		PrintMessage("Token is null");
		value = 0;
		return OPT_INT;
	}
	if(token[0] == '@')
	{
		value = CreateString(token);
		return OPT_APPINT;
	}
	int var = GetVariableIndex(token);
	if(var >= 0)
	{
		value = var;
		return OPT_VAR;
	}
	value = atoi(token);
	return OPT_INT;
}

int ScriptDef :: ResolveComparisonType(const char *token)
{
	static const char *name[6]  = {   "=",    "!=",    "<",    "<=",    ">",    ">=" };
	static const int value[6] = {CMP_EQ, CMP_NEQ, CMP_LT, CMP_LTE, CMP_GT, CMP_GTE };
	for(int i = 0; i < 6; i++)
		if(strcmp(token, name[i]) == 0)
			return value[i];

	return CMP_INV;
}

void ScriptDef :: SetScriptSpeed(const char *token)
{
	int amount = atoi(token);
	if(amount < 1)
		amount = 1;
	scriptSpeed = 1;
}

bool ScriptDef :: CanIdle(void)
{
	return (scriptIdleSpeed > 0);
}

bool ScriptDef :: UseEventQueue(void)
{
	return queueExternalJumps;
}

void ScriptDef :: SetMetaDataBase(const char *opname, ScriptCompiler &compileData)
{
	const STRINGLIST &tokens = compileData.mTokens;  //Alias so we don't have to change the rest of the code.
	if(strcmp(opname, "#name") == 0)
	{
		if(tokens.size() >= 2)
			scriptName = tokens[1];
	}
	else if(strcmp(opname, "#symbol") == 0)
	{
		if(tokens.size() >= 3)
			compileData.AddSymbol(tokens[1], tokens[2]);
	}
	else if(strcmp(opname, "#speed") == 0)
	{
		if(tokens.size() >= 2)
			scriptSpeed = atoi(tokens[1].c_str());
	}
	else if(strcmp(opname, "#idlespeed") == 0)
	{ 
		if(tokens.size() >= 2)
			scriptIdleSpeed = atoi(tokens[1].c_str());
	}
	else if(strcmp(opname, "#queue_jumps") == 0)
	{ 
		queueCallStyle = CALLSTYLE_CALL;
		queueExternalJumps = true;
		if(tokens.size() >= 2)
		{
			if(tokens[1].compare("goto") == 0)
				queueCallStyle = CALLSTYLE_GOTO;
		}
	}
	else if(strcmp(opname, "#flag") == 0)
	{
		if(tokens.size() >= 3)
		{
			const char *flagName = tokens[1].c_str();
			int value = atoi(tokens[2].c_str());
			unsigned int flag = 0;
			if(strcmp(flagName, "report_end") == 0)
				flag = FLAG_REPORT_END;
			else if(strcmp(flagName, "report_label") == 0)
				flag = FLAG_REPORT_LABEL;
			else if(strcmp(flagName, "report_all") == 0)
				flag = FLAG_REPORT_ALL;
			else if(strcmp(flagName, "bits") == 0)
				flag = FLAG_BITS;
			else
				PrintMessage("Unknown flag [%s] [%s line %d]", flagName, compileData.mSourceFile, compileData.mLineNumber);
			SetFlag(flag, value);
		}
	}
	else
	{
		SetMetaDataDerived(opname, compileData);
	}
}

void ScriptDef :: SetFlag(unsigned int flag, unsigned int value)
{
	if(flag == FLAG_BITS)
	{
		mFlags = value;
		return;
	}
	if(value)
		mFlags |= flag;
	else
		mFlags &= (~(flag));
}

bool ScriptDef :: HasFlag(unsigned int flag)
{
	return ((mFlags & flag) != 0);
}

void ScriptDef :: SetMetaDataDerived(const char *opname, ScriptCompiler &compileData)
{
	//const STRINGLIST &tokens = compileData.mTokens;  //Alias so we don't have to change the rest of the code.
	PrintMessage("Unhandled metadata token [%s] (%s line %d)", opname, compileData.mSourceFile, compileData.mLineNumber);
}



//Perform special handling for certain commands here, with parameter counts or behavior that cannot
//properly fit into the "command [param] ..." line format
//Return false if there was no handler for the operative token.
//virtual : Override in derived class if necessary.
bool ScriptDef :: HandleAdvancedCommand(const char *commandToken, ScriptCompiler &compileData)
{
	return false;
}

ScriptPlayer :: ScriptPlayer()
{
	def = NULL;
	curInst = 0;
	active = false;
	nextFire = 0;
	advance = 0;
}

ScriptPlayer :: ~ScriptPlayer()
{
}

void ScriptPlayer :: Initialize(ScriptDef *defPtr)
{
	def = defPtr;
	FullReset();
}

void ScriptPlayer :: RunScript(void)
{
	while(active == true)
		RunSingleInstruction();
}

bool ScriptPlayer :: RunSingleInstruction(void)
{
	//Return true if the script is interrupted or terminated.
	if(active == false)
		return true;

	if(curInst >= (int)def->instr.size())
	{
		PrintMessage("[ERROR] Instruction past end of script (%d of %d)", curInst, def->instr.size());
		active = false;
		return true;
	}
	if(g_ServerTime < nextFire)
		return true;

	bool breakScript = false;
	advance = 1;

	OpData *instr = &def->instr[curInst];

	switch(instr->opCode)
	{
	case OP_END:
		HaltExecution();
		breakScript = true;
		break;
	case OP_GOTO:
		//printf("Jumping:\n");
		//g_Log.AddMessageFormat("Jump Before/After: %d to %d", curInst, def->instr[curInst].param1);
		curInst = instr->param1;
		advance = 0;
		/*
		{
			int t = def->instr[curInst].param1;
			if(t < 0 || t >= (int)def->label.size())
				g_Log.AddMessageFormat("Index out of range: %d/%d", t, def->label.size());
			else
				g_Log.AddMessageFormat("%d = %s,%d", t, def->label[t].name.c_str(), def->label[t].instrOffset);
		}
		*/
		break;
	case OP_RESETGOTO:
		ResetGoto(def->instr[curInst].param1);
		advance = 0;
		break;
	case OP_PRINT:
		PrintMessage("%s", def->stringList[def->instr[curInst].param1].c_str());
		break;
	case OP_PRINTVAR:
		PrintMessage("var[%s]=%d", def->varName[instr->param1].c_str(), GetVarValue(instr->param1));
		break;
	case OP_PRINTAPPVAR:
		PrintMessage("appvar[%s]=%d", GetStringPtr(instr->param1), GetApplicationPropertyAsInteger(GetStringPtr(instr->param1)));
		break;
	case OP_PRINTINTARR:
		if(VerifyIntArrayIndex(instr->param1) >= 0)
			intArray[instr->param1].DebugPrintContents();
		break;
	case OP_WAIT:
		nextFire = g_ServerTime + instr->param1;
		breakScript = true;
		break;
	case OP_PUSHVAR:
		int value;
		value = GetVarValue(instr->param1);
		PushVarStack(value);
		break;
	case OP_PUSHAPPVAR:
		PushVarStack(GetApplicationPropertyAsInteger(GetStringPtr(instr->param1)));
		break;
	case OP_PUSHINT:
		PushVarStack(instr->param1);
		break;
	case OP_POP:
		SetVar(instr->param1, PopVarStack());
		break;
	case OP_CMP:
		int left, right, cmp;
		bool result;
		result = false;
		left = PopVarStack();
		right = PopVarStack();
		cmp = instr->param1;
		//PrintMessage("IF %d (%d) %d", left, cmp, right);
		switch(cmp)
		{
		case CMP_EQ: result = (left == right); break;
		case CMP_NEQ: result = (left != right); break;
		case CMP_LT: result = (left < right); break;
		case CMP_LTE: result = (left <= right); break;
		case CMP_GT: result = (left > right); break;
		case CMP_GTE: result = (left >= right); break;
		};
		if(result == false)
			advance = instr->param2;
		break;
	case OP_INC:
		vars[instr->param1]++;
		break;
	case OP_DEC:
		vars[instr->param1]--;
		break;
	case OP_SET:
		SetVar(instr->param1, instr->param2);
		break;
	case OP_COPYVAR:
		SetVar(instr->param2, GetVarValue(instr->param1));
		break;
	case OP_GETAPPVAR:
		SetVar(instr->param2, GetApplicationPropertyAsInteger(GetStringPtr(instr->param1)));
		break;
	case OP_CALL:
		Call(instr->param1);
		advance = 0;
		break;
	case OP_RETURN:
		curInst = PopCallStack();
		advance = 0;
		break;
	case OP_JMP:
		curInst = instr->param1;
		advance = 0;
		break;
	case OP_IARRAPPEND:
		{
		int value = PopVarStack();
		if(VerifyIntArrayIndex(instr->param1) >= 0)
			intArray[instr->param1].Append(value);
		}
		break;
	case OP_IARRDELETE:
		{
		int index = PopVarStack();
		if(VerifyIntArrayIndex(instr->param1) >= 0)
			intArray[instr->param1].RemoveByIndex(index);
		}
		break;
	case OP_IARRVALUE:
		{
		int index = PopVarStack();
		if(VerifyIntArrayIndex(instr->param1) >= 0)
			SetVar(instr->param3, intArray[instr->param1].GetValueByIndex(index));
		}
		break;
	case OP_IARRCLEAR:
		if(VerifyIntArrayIndex(instr->param1) >= 0)
			intArray[instr->param1].Clear();
		break;
	case OP_IARRSIZE:
		{
			int size = 0;
			if(VerifyIntArrayIndex(instr->param1) >= 0)
				size = intArray[instr->param1].Size();
			SetVar(instr->param2, size);
		}
		break;
	case OP_QUEUEEVENT:
		QueueEvent(GetStringPtr(instr->param1), (unsigned long)instr->param2);
		break;
	case OP_EXECQUEUE:
		//If we performed a jump, we don't want to advance because the instruction index was
		//already changed by the jump.
		if(ExecQueue() == true)
			advance = 0;
		break;
	default:
		RunImplementationCommands(instr->opCode);
		break;
	}

	curInst += advance;
	return breakScript;
}

void ScriptPlayer :: RunImplementationCommands(int opcode)
{
	switch(opcode)
	{
	case OP_NOP:
		break;
	default:
		PrintMessage("Unidentified op type: %d", def->instr[curInst].opCode);
		break;
	}
}

void ScriptPlayer :: RunUntilWait(void)
{
	while(active == true && (g_ServerTime >= nextFire))
	{
		if(RunSingleInstruction() == true)
			break;
	}
}

void ScriptPlayer :: RunAtSpeed(int maxCommands)
{
	int maxCount = maxCommands;
	if(def->scriptSpeed > maxCount)
		maxCount = def->scriptSpeed;

	int count = 0;
	while(active == true && (g_ServerTime >= nextFire))
	{
		if(RunSingleInstruction() == true)
			break;
		count++;
		if(count >= maxCount)
			break;
	}
}

bool ScriptPlayer :: CanRunIdle(void)
{
	return def->CanIdle();
}

bool ScriptPlayer :: JumpToLabel(const char *name)
{
	if(def->UseEventQueue() == true)
	{
		QueueEvent(name, 0);
		return true;
	}
	return PerformJumpRequest(name, ScriptDef::CALLSTYLE_GOTO);
}

bool ScriptPlayer::PerformJumpRequest(const char *name, int callStyle)
{
	int index = def->GetLabelIndex(name);
	if(index >= 0)
	{
		//Make sure we're still running.
		active = true;
		if(callStyle == ScriptDef::CALLSTYLE_GOTO)
		{
			ResetGoto(def->label[index].instrOffset);
		}
		else
		{
			Call(def->label[index].instrOffset);
		}
		return true;
	}
	else
	{
		if(def->HasFlag(ScriptDef::FLAG_REPORT_LABEL))
			PrintMessage("Label [%s] not found in script [%s]", name, def->scriptName.c_str());

		//Just need to determine if the script should be halted on failure.  If it doesn't use
		//an event queue, it probably needs to be stopped.
		if(def->UseEventQueue() == false)
		{
			HaltExecution();
		}
	}
	return false;
}

void ScriptPlayer :: HaltExecution(void)
{
	active = false;
	scriptEventQueue.clear();
	if(def->HasFlag(ScriptDef::FLAG_REPORT_END))
		PrintMessage("Script [%s] has ended", def->scriptName.c_str());
}

void ScriptPlayer :: Call(int targetInstructionIndex)
{
	//When we return, we want to be on the following instruction, not the call.
	PushCallStack(curInst + 1);
	curInst = targetInstructionIndex;
}

void ScriptPlayer :: ResetGoto(int targetInstructionIndex)
{
	callStack.clear();
	varStack.clear();
	curInst = targetInstructionIndex;
	nextFire = g_ServerTime;
}


//Override this function to substitute application-defined variables.
int ScriptPlayer :: GetApplicationPropertyAsInteger(const char *propertyName)
{
	if(propertyName == NULL)
		return 0;
	
	return 0;
}

void ScriptPlayer :: PushVarStack(int value)
{
	if(varStack.size() > MAX_STACK_SIZE)
		PrintMessage("[ERROR] Script error: PushVarStack() stack is full [script: %s]", def->scriptName.c_str());
	else
		varStack.push_back(value);
}

int ScriptPlayer :: PopVarStack(void)
{
	int retval = 0;
	if(varStack.size() == 0)
		PrintMessage("[ERROR] Script error: PopVarStack() stack is empty [script: %s]", def->scriptName.c_str());
	else
	{
		retval = varStack[varStack.size() - 1];
		varStack.pop_back();
	}
	return retval;
}

void ScriptPlayer :: PushCallStack(int value)
{
	if(callStack.size() > MAX_STACK_SIZE)
		PrintMessage("[ERROR] Script error: PushCallStack() stack is full [script: %s]", def->scriptName.c_str());
	else
		callStack.push_back(value);
}

int ScriptPlayer :: PopCallStack(void)
{
	int retval = 0;
	if(callStack.size() == 0)
		PrintMessage("[ERROR] Script error: PopCallStack() stack is empty [script: %s]", def->scriptName.c_str());
	else
	{
		retval = callStack[callStack.size() - 1];
		callStack.pop_back();
	}
	return retval;
}

void ScriptPlayer :: QueueEvent(const char *labelName, unsigned long fireDelay)
{
	if(scriptEventQueue.size() >= MAX_QUEUE_SIZE)
	{
		PrintMessage("[ERROR] Script error: QueueEvent() list is full [script: %s]", def->scriptName.c_str());
		return;
	}

	unsigned long fireTime = g_ServerTime + fireDelay;

	//If a event label is already registered, just update the fire time.
	for(size_t i = 0; i < scriptEventQueue.size(); i++)
	{
		if(scriptEventQueue[i].mLabel.compare(labelName) == 0)
		{
			scriptEventQueue[i].mFireTime = fireTime;
			return;
		}
	}

	//Not found, add a new event.
	scriptEventQueue.push_back(ScriptEvent(labelName, fireTime));
}

bool ScriptPlayer :: ExecQueue(void)
{
	for(size_t i = 0; i < scriptEventQueue.size(); i++)
	{
		if(g_ServerTime >= scriptEventQueue[i].mFireTime)
		{
			PerformJumpRequest(scriptEventQueue[i].mLabel.c_str(), def->queueCallStyle);
			scriptEventQueue.erase(scriptEventQueue.begin() + i);
			return true;
		}
	}
	return false;
}

const char * ScriptPlayer :: GetStringPtr(int index)
{
	static const char *NULL_RESPONSE = "<null>";

	if(index < 0 || index >= static_cast<int>(def->stringList.size()))
	{
		PrintMessage("Script error: string index out of range [%d] for script [%s]", index, def->scriptName.c_str());
		return NULL_RESPONSE;
	}
	return def->stringList[index].c_str();
}

int ScriptPlayer :: GetVarValue(int index)
{
	int retval = 0;
	if(index < 0 || index >= (int)vars.size())
		PrintMessage("Script error: variable index out of range [%d] for script [%s]", index, def->scriptName.c_str());
	else
		retval = vars[index];

	return retval;
}

const char * ScriptPlayer :: GetStringTableEntry(int index)
{
	const char *retval = "";
	if(index < 0 || index >= (int)def->stringList.size())
		PrintMessage("Script error: string index out of range [%d] for script [%s]", index, def->scriptName.c_str());
	else
		retval = def->stringList[index].c_str();

	return retval;
}

int ScriptPlayer :: VerifyIntArrayIndex(int index)
{
	if(index < 0 || index >= (int)def->mIntArray.size())
	{
		PrintMessage("Script error: IntArray index out of range [%d] for script [%s]", index, def->scriptName.c_str());
		return -1;
	}
	return index;
}

void ScriptPlayer :: SetVar(unsigned int index, int value)
{
	if(index >= vars.size())
	{
		PrintMessage("[ERROR] SetVar() index [%d] is outside range [%d]", index, vars.size());
		return;
	}
	vars[index] = value;
}

void ScriptPlayer :: FullReset(void)
{
	if(def == NULL)
	{
		active = false;
		return;
	}

	curInst = 0;
	active = true;
	nextFire = 0;

	varStack.clear();
	callStack.clear();

	vars.clear();
	vars.resize(def->varName.size(), 0);
	for(size_t i = 0; i < vars.size(); i++)
		vars[i] = 0;

	intArray.clear();
	intArray.assign(def->mIntArray.begin(), def->mIntArray.end());
	for(size_t i = 0; i < intArray.size(); i++)
		intArray[i].Clear();

	scriptEventQueue.clear();
}

bool ScriptPlayer :: IsWaiting(void)
{
	return (g_ServerTime > nextFire);
}

void PrintMessage(const char *format, ...)
{
	char messageBuf[2048];

	va_list args;
	va_start (args, format);
	vsnprintf(messageBuf, sizeof(messageBuf) - 1, format, args);
	va_end (args);

		g_Log.AddMessageFormat("%s", messageBuf);
}


BlockData::BlockData()
{
	mLineNumber = 0;
	mInstIndex = 0;
	mInstIndexElse = 0;
	mNestLevel = 0;
	mResolved = false;
	mUseElse = false;
}

ScriptCompiler::ScriptCompiler()
{
	mCurrentNestLevel = 0;
	mLastNestLevel = 0;
	mSourceFile = "<no file>";
	mLineNumber = 0;
	mInlineBeginInstr = 0;
}

void ScriptCompiler::OpenBlock(int lineNumber, int instructionIndex)
{
	BlockData newBlock;
	newBlock.mLineNumber = lineNumber;
	newBlock.mInstIndex = instructionIndex;
	newBlock.mNestLevel = mCurrentNestLevel++;
	newBlock.mResolved = false;

	mBlockData.push_back(newBlock);
}

bool ScriptCompiler::CloseBlock(void)
{
	int size = (int)mBlockData.size();
	for(int i = size - 1; i >= 0; i--)
	{
		if(mBlockData[i].mResolved == true)
			continue;

		mBlockData[i].mResolved = true;
		mCurrentNestLevel--;
		return true;
	}
	return false;
}

BlockData* ScriptCompiler::GetLastUnresolvedBlock(void)
{
	int size = (int)mBlockData.size();
	for(int i = size - 1; i >= 0; i--)
	{
		if(mBlockData[i].mResolved == true)
			continue;

		return &mBlockData[i];
	}
	return NULL;
}

void ScriptCompiler::AddSymbol(const std::string &key, const std::string &value)
{
	mSymbols[key] = value;
}

bool ScriptCompiler::HasSymbol(const std::string &token)
{
	std::map<std::string, std::string>::iterator it;
	it = mSymbols.find(token);
	if(it == mSymbols.end())
		return false;
	return true;
}

void ScriptCompiler::CheckSymbolReplacements(void)
{
	if(mSymbols.size() == 0)
		return;

	for(size_t i = 0; i < mTokens.size(); i++)
	{
		if(HasSymbol(mTokens[i]) == true)
		{
			mTokens[i] = mSymbols[mTokens[i]];
		}
	}
}

bool ScriptCompiler :: ExpectTokens(size_t count, const char *op, const char *desc)
{
	if(mTokens.size() != count)
	{
		PrintMessage("[%s] expects parameters [%s] [%s line %d]", op, desc, mSourceFile, mLineNumber);
		return false;
	}
	return true;
}

void ScriptCompiler :: AddPendingLabelReference(int instructionIndex)
{
	mPendingLabelReference.push_back(instructionIndex);
}


IntArray::IntArray()
{
}

IntArray::IntArray(const char *intArrName)
{
	name = intArrName;
}

void IntArray::Clear(void)
{
	arrayData.clear();
}

int IntArray::Size(void)
{
	return static_cast<int>(arrayData.size());
}

void IntArray::Append(int value)
{
	if(arrayData.size() >= MAX_ARRAY_DATA_SIZE)
	{
		PrintMessage("IntArray [%s] cannot append more than %d elements", name.c_str(), MAX_ARRAY_DATA_SIZE);
		return;
	}
	arrayData.push_back(value);
}

void IntArray::RemoveByIndex(int index)
{
	if(VerifyIndex(index) == true)
	{
		arrayData.erase(arrayData.begin() + index);
	}
}

int IntArray::GetValueByIndex(int index)
{
	if(VerifyIndex(index) == true)
	{
		return arrayData[index];
	}
	return 0;
}

int IntArray::GetIndexByValue(int value)
{
	for(size_t i = 0; i < arrayData.size(); i++)
		if(arrayData[i] == value)
			return (int)i;
	return -1;
}

bool IntArray::VerifyIndex(int index)
{
	if(index < 0 || index >= (int)arrayData.size())
	{
		PrintMessage("IntArray [%s] index [%d] out of range", name.c_str(), index);
		return false;
	}
	return true;
}

void IntArray::DebugPrintContents(void)
{
	char buffer[16];
	std::string str = "[";
	for(size_t i = 0; i < arrayData.size();  i++)
	{
		if(i > 0)
			str += ",";
		sprintf(buffer, "%d", arrayData[i]);
		str += buffer;
	}
	str += "]";
	PrintMessage("IntArray[%s]=%s", name.c_str(), str.c_str());
}

ScriptEvent::ScriptEvent(const char *label, unsigned long fireTime)
{
	mLabel = label;
	mFireTime = fireTime;
}

//namespace ScriptCore
}
