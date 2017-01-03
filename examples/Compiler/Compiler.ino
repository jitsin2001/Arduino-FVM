/**
 * @file FVM/Compiler.ino
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2016, Mikael Patel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 *
 * @section Description
 * Basic interactive compiler for the Forth Virtual Machine (FVM).
 * Compiles forth definitions, statements and generates virtual
 * machine code (C++).
 */

#include "FVM.h"

FVM_COLON(0, FORWARD_MARK, "mark>")
// : mark> ( -- addr ) here 0 c, ;
  FVM_OP(HERE),
  FVM_OP(ZERO),
  FVM_OP(C_COMMA),
  FVM_OP(EXIT)
};

FVM_COLON(1, FORWARD_RESOLVE, "resolve>")
// : resolve> ( addr -- ) here over - swap c! ;
  FVM_OP(HERE),
  FVM_OP(OVER),
  FVM_OP(MINUS),
  FVM_OP(SWAP),
  FVM_OP(C_STORE),
  FVM_OP(EXIT)
};

FVM_COLON(2, BACKWARD_MARK, "<mark")
// : <mark ( -- addr ) here ;
  FVM_OP(HERE),
  FVM_OP(EXIT)
};

FVM_COLON(3, BACKWARD_RESOLVE, "<resolve")
// : <resolve ( addr -- ) here - c, ;
  FVM_OP(HERE),
  FVM_OP(MINUS),
  FVM_OP(C_COMMA),
  FVM_OP(EXIT)
};

FVM_COLON(4, IF, "if")
// : if ( -- addr ) compile (0branch) mark> ; immediate
  FVM_OP(COMPILE),
  FVM_OP(ZERO_BRANCH),
  FVM_CALL(FORWARD_MARK),
  FVM_OP(EXIT)
};

FVM_COLON(5, THEN, "then")
// : then ( addr -- ) resolve> ; immediate
  FVM_CALL(FORWARD_RESOLVE),
  FVM_OP(EXIT)
};

FVM_COLON(6, ELSE, "else")
// : else ( addr1 -- addr2 ) compile (branch) mark> swap resolve> ; immediate
  FVM_OP(COMPILE),
  FVM_OP(BRANCH),
  FVM_CALL(FORWARD_MARK),
  FVM_OP(SWAP),
  FVM_CALL(FORWARD_RESOLVE),
  FVM_OP(EXIT)
};

FVM_COLON(7, BEGIN, "begin")
// : begin ( -- addr ) <mark ; immediate
  FVM_CALL(BACKWARD_MARK),
  FVM_OP(EXIT)
};

FVM_COLON(8, AGAIN, "again")
// : again ( addr -- ) compile (branch) <resolve ; immediate
  FVM_OP(COMPILE),
  FVM_OP(BRANCH),
  FVM_CALL(BACKWARD_RESOLVE),
  FVM_OP(EXIT)
};

FVM_COLON(9, UNTIL, "until")
// : until ( addr -- ) compile (0branch) <resolve ; immediate
  FVM_OP(COMPILE),
  FVM_OP(ZERO_BRANCH),
  FVM_CALL(BACKWARD_RESOLVE),
  FVM_OP(EXIT)
};

#if 0
FVM_COLON(10, WHILE, "while")
// : while ( addr1 -- addr1 addr2 ) compile (0branch) mark> ; immediate
  FVM_OP(COMPILE),
  FVM_OP(ZERO_BRANCH),
  FVM_CALL(FORWARD_MARK),
  FVM_OP(EXIT)
};
#else
const int WHILE = 10;
const char WHILE_PSTR[] PROGMEM = "while";
# define WHILE_CODE IF_CODE
#endif

FVM_COLON(11, REPEAT, "repeat")
// : repeat (addr1 addr2 -- ) swap [compile] again resolve> ; immediate
  FVM_OP(SWAP),
  FVM_CALL(AGAIN),
  FVM_CALL(FORWARD_RESOLVE),
  FVM_OP(EXIT)
};

const FVM::code_P FVM::fntab[] PROGMEM = {
  FORWARD_MARK_CODE,
  FORWARD_RESOLVE_CODE,
  BACKWARD_MARK_CODE,
  BACKWARD_RESOLVE_CODE,
  IF_CODE,
  THEN_CODE,
  ELSE_CODE,
  BEGIN_CODE,
  AGAIN_CODE,
  UNTIL_CODE,
  WHILE_CODE,
  REPEAT_CODE
};

const str_P FVM::fnstr[] PROGMEM = {
  (str_P) FORWARD_MARK_PSTR,
  (str_P) FORWARD_RESOLVE_PSTR,
  (str_P) BACKWARD_MARK_PSTR,
  (str_P) BACKWARD_RESOLVE_PSTR,
  (str_P) IF_PSTR,
  (str_P) THEN_PSTR,
  (str_P) ELSE_PSTR,
  (str_P) BEGIN_PSTR,
  (str_P) AGAIN_PSTR,
  (str_P) UNTIL_PSTR,
  (str_P) WHILE_PSTR,
  (str_P) REPEAT_PSTR,
  0
};

// Data area for the shell
uint8_t data[128];
bool compiling = false;
uint8_t* latest = data;

// Forth virtual machine and task
FVM fvm(data);
FVM::task_t task(Serial);

void setup()
{
  Serial.begin(57600);
  while (!Serial);
  Serial.println(F("FVM/Compiler: started [Newline]"));
}

void loop()
{
  char buffer[32];
  int op;

  // Scan buffer for a single word or number
  fvm.scan(buffer, task);
  op = fvm.lookup(buffer);

  // Check for special words or literals
  if (op < 0) {

    // Check for start of definition
    if (!strcmp_P(buffer, PSTR(":")) && !compiling) {
      fvm.scan(buffer, task);
      fvm.compile((FVM::code_t) 0);
      fvm.compile(buffer);
      compiling = true;
    }

    // Check for comment
    else if (!strcmp_P(buffer, PSTR("("))) {
      while (Serial.read() != ')') yield();
    }

    // Check for immediate and ignore
    else if (!strcmp_P(buffer, PSTR("immediate"))) {
    }

    // Check for code generation
    else if (!strcmp_P(buffer, PSTR("codegen"))) {
      codegen(Serial);
      fvm.dp(data);
      latest = data;
    }

    // Check for call
    else if ((op = lookup(buffer)) > 0) {
      fvm.compile(-op);
    }

    // Check for end of definition
    else if (!strcmp_P(buffer, PSTR(";")) && compiling) {
      fvm.compile(FVM::OP_EXIT);
      *latest = fvm.dp() - latest - 1;
      latest = fvm.dp();
      compiling = false;
    }

    // Assume number (should check)
    else {
      int value = atoi(buffer);
      if (compiling) {
	if (value < -128 || value > 127) {
	  fvm.compile(FVM::OP_LIT);
	  fvm.compile(value);
	  fvm.compile(value >> 8);
	}
	else {
	  fvm.compile(FVM::OP_CLIT);
	  fvm.compile(value);
	}
      }
      else {
	task.push(value);
      }
    }
  }

  // Compile operation
  else if (op < 128 && compiling) {
    fvm.compile(op);
  }
  else {
    fvm.execute(op, task);
  }
}

int lookup(const char* s)
{
  int res = 1;
  uint8_t* dp = data;
  while (dp < fvm.dp()) {
    if (!strcmp(s, (const char*) dp + 1)) return (res);
    dp += ((uint8_t) *dp) + 1;
    res += 1;
  }
  return (-1);
}

void codegen(Stream& ios)
{
  int nr, last;
  uint8_t* dp;

  // Generate function name strings and code
  dp = data;
  nr = 0;
  while (dp < fvm.dp()) {
    uint8_t length = *dp++;
    ios.print(F("const char WORD"));
    ios.print(nr);
    ios.print(F("_PSTR[] PROGMEM = \""));
    ios.print((char*) dp);
    ios.println(F("\";"));
    ios.print(F("const FVM::code_t WORD"));
    ios.print(nr);
    ios.print(F("_CODE[] PROGMEM = {\n  "));
    uint8_t n = strlen((char*) dp) + 1;
    dp += n;
    for (;n < length; n++) {
      int8_t code = (int8_t) *dp++;
      ios.print(code);
      if (code != 0) ios.print(F(", "));
    }
    ios.println();
    ios.println(F("};"));
    nr += 1;
  }
  last = nr;

  // Generate function code table
  nr = 0;
  dp = data;
  ios.println(F("const FVM::code_P FVM::fntab[] PROGMEM = {"));
  while (dp < fvm.dp()) {
    uint8_t length = *dp++;
    ios.print(F("  WORD"));
    ios.print(nr++);
    ios.print(F("_CODE"));
    if (nr != last) ios.println(','); else ios.println();
    dp += length;
  }
  ios.println(F("};"));

  // Generate function string table
  nr = 0;
  dp = data;
  ios.println(F("const str_P FVM::fnstr[] PROGMEM = {"));
  while (dp < fvm.dp()) {
    uint8_t length = *dp++;
    ios.print(F("  (str_P) WORD"));
    ios.print(nr++);
    ios.println(F("_PSTR,"));
    dp += length;
  }
  ios.println(F("  0"));
  ios.println(F("};"));
}