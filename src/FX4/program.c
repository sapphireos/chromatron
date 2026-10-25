
// #include "program.h"
// #include "hash32.h"
// #include "bytecode.h"

// #include <stdio.h>
// #include <string.h>

/*
 * 
 * Stuff we need:
 * constants table
 * function scoping
 * default function
 * function locals
 * alloc globals to stack locations
 * type checking/conversions
 * strings
 * call/return
 * jumps
 * labels
 * 
 * 





pixels[1].hue = array.val[2];

EXPR SHUNTING YARD:
array path
    LOAD: pixels
    PUSH: 1
    RBRACKET
    LOAD: hue
    DOT
    LVALUE: =
    LOAD: array
    LOAD: val
    PUSH: 2
    RBRACKET
    DOT
    ASSIGN
    SEMICOLON
EXPR_END


pixels[1].hue = array.val[2];

Run tokens as a stack  machine to operate the backend
of the compiler?


1.  LOAD: pixels
        IDENT(pixels) -> TOS | IDENT(pixels)

2.  PUSH: 1
        NUMBER(1) -> TOS | NUMBER(1) IDENT(pixels)

3.  RBRACKET
        pop 2
        val = ref(pixels) + number(1)
        Ref(pixels[1]) -> TOS | Ref(pixels[1])

4.  LOAD: hue
        IDENT(hue) -> TOS | IDENT(hue) Ref(pixels[1])

5.  DOT
        pop 2
        Ref(pixels[1].hue) -> TOS | Ref(pixels[1].hue)

6.  LVALUE: =
        Not sure if we need this yet....

7.  LOAD: array
        IDENT(array) -> TOS | IDENT(array) Ref(pixels[1].hue)

8.  LOAD: val
        IDENT(val) -> TOS |  IDENT(val) IDENT(array) Ref(pixels[1].hue)

9.  PUSH: 2
        NUMBER(2) -> TOS |  NUMBER(2) IDENT(val) IDENT(array) Ref(pixels[1].hue)

10. RBRACKET
        pop 2
        val = ref(val) + number(2)
        Ref(val[2]) -> TOS | Ref(val[2]) IDENT(array) Ref(pixels[1].hue)

11. DOT
        pop 2
        Ref(array.val[2]) -> TOS |  Ref(array.val[2]) Ref(pixels[1].hue)
        
12. ASSIGN
        pop 2    
        load from mem ref: array.val[2] -> temp
        temp -> store to mem ref: pixels[1].hue

13. SEMICOLON
        end of expression marker.
        might be a nop.

pixels[1].hue = array.val[2];




a = 1 + 2;

1. LOAD: a
    IDENT(a) -> TOS | IDENT(a)

2. LVALUE: =
    Signals an LVALUE.
    Might be a nop.

3. PUSH: 1
    NUMBER(1) -> TOS | NUMBER(1) IDENT(a)

4. PUSH: 2
    NUMBER(2) -> TOS | NUMBER(2) NUMBER(1) IDENT(a)

5. ADD
    pop 2
    val = 1 + 2
    # RHS is TOS, LHS is TOS-1, so the sides switch.  LHS was added first.

    NUMBER(3) -> TOS | NUMBER(3) IDENT(a)

6. ASSIGN
    pop 2
    load tos number(3) -> temp
    temp -> store to variable: a
    
7. SEMICOLON
        end of expression marker.
        might be a nop.
    



fn meow(number a, number b) number{

    print(a + b);

    return a * b;
}




1. FUNC: meow:
    ARGS:
        DECLARE: number: a
        DECLARE: number: b
        RETURNS: number

2. OPEN_BLOCK
    assign new block ID to current function
    the current block ID can be used to track
    when the function is closed.

3. LOAD: a
    IDENT(a) -> TOS | IDENT(a)

4. LOAD: b
    IDENT(b) -> TOS | IDENT(b) IDENT(a)

5. ADD
    pop 2
    val = a + b
    VALUE(a + b) -> TOS | VALUE(a + b)

6. LCALL: print
    pop1
    arg = VALUE(a + b)
    C LIBRARY CALL: print(arg) -> print(VALUE(a + b)) -> "3"
    print is operand to LCALL (so it isn't on the stack)
    print does not return anything.
    however an lcall (or call) may have
    a return value (or maybe values?)
    (supporting arrays is.... kinda easy with a stack, but memory use is a problem at scale)

7. SEMICOLON
        end of expression marker.
        might be a nop.

8.  LOAD: a
    IDENT(a) -> TOS | IDENT(a)

9.  LOAD: b
    IDENT(b) -> TOS | IDENT(b) IDENT(a)

10. MUL
    pop 2
    val = a * b
    VALUE(a * b) -> TOS | VALUE(a * b)

11. SEMICOLON
        end of expression marker.
        might be a nop.

12. RETURN:
    pop TOS
    val = VALUE(a * b)
    
    do whatever it is we do with return values!

    can this be as easy as... leave it on the stack?
    Then we return to the caller + 1, and TOS
    is the returned value ready to use.

    Need to think about return addresses...
    A call stack is certainly the easy way to do it.
    
    Need to run a function call by hand too to get the other
    side of it....    
    
    TOS | -nil-


13. CLOSE_BLOCK
    Close out code for this function.
    Verify block ID/level matches.
    Is does ID/level mean anything?  Just level?
    Next operation needs to be END_FUNC (peek).

14. END_FUNC
    verify block level has returned to 0 by this point
    this catches a missing closure
    Close out adding code to this function



a = meow(1, 2);

1.  LOAD: a
        IDENT(a) -> TOS | IDENT(a)

2.  LVALUE: =
        Signals an LVALUE.
        Might be a nop.

3.  PUSH: 1
        NUMBER(1) -> TOS | NUMBER(1) IDENT(a)

4.  PUSH: 2
        NUMBER(2) -> TOS | NUMBER(2) NUMBER(1) IDENT(a)

5.  CALL: meow
        type checking here:
        look up meow, check number of args, stack must have
        at least that many.
        also check types.
        and return, type, etc.

        operate call operatus.
        pop 2
        we will assume meow adds the 2 numbers
        VALUE(1 + 2) -> TOS | VALUE(1 + 2) IDENT(a)

6.  ASSIGN
        pop 2
        load tos value(1 + 2) -> temp
        temp -> store to variable: a

7.  SEMICOLON
        end of expression marker.
        might be a nop.        



number my_num = 1 + 2;
 
1.  DECLARE: number: my_num
        Create variable my_num of type number
        Check scope:
            If level 0, global, add to globals.
            If >0, local, add to current function locals.
        
2.  LOAD: my_num
        IDENT(my_num) -> TOS | IDENT(my_num)

3.  LVALUE: =
        Signals an LVALUE.
        Might be a nop.

4.  PUSH: 1
        NUMBER(1) -> TOS | NUMBER(1) IDENT(my_num)

5.  PUSH: 2
        NUMBER(2) -> TOS | NUMBER(2) NUMBER(1) IDENT(my_num)

6.  ADD
        pop 2
        val = 1 + 2
        VALUE(1 + 2) -> TOS | VALUE(1 + 2) IDENT(my_num)

7.  ASSIGN
        pop 2
        load tos value(1 + 2) -> temp
        temp -> store to variable: my_num

8.  SEMICOLON
        end of expression marker.
        might be a nop.        


function my_func = meow;

1.  DECLARE: function: my_func
        Create variable my_func of type function
        - this is a function reference
        Check scope:
            If level 0, global, add to globals.
            If >0, local, add to current function locals.

2.  LOAD: my_func
        IDENT(my_func) -> TOS | IDENT(my_func)

3.  LVALUE: =
        Signals an LVALUE.
        Might be a nop.

4.  LOAD: meow
        IDENT(meow) -> TOS | IDENT(meow) IDENT(my_func)

5.  ASSIGN
        pop 2
        load tos ident(meow) -> temp
        temp -> store to variable: ident(my_func)


6.  SEMICOLON
        end of expression marker.
        might be a nop.        



a += 1;

1. LOAD: a
    IDENT(a) -> TOS | IDENT(a)

2. PUSH: 1
    NUMBER(1) -> TOS | NUMBER(1) IDENT(a)

3. ADD_ASSIGN
    pop 2
    val = number(1)
    store val -> variable ident(a)

4. SEMICOLON
        end of expression marker.
        might be a nop.        



pixels.hue += 1;

1. LOAD: pixels
    IDENT(pixels) -> TOS | IDENT(pixels)

2. LOAD: hue
    IDENT(hue) -> TOS | IDENT(hue) IDENT(pixels)

3. DOT
    pop 2
    Ref(pixels.hue) -> TOS | Ref(pixels.hue)

4. PUSH: 1
    NUMBER(1) -> TOS | NUMBER(1) Ref(pixels.hue)

5. ADD_ASSIGN
    pop 2
    val = number(1)
    store val -> variable ref(pixels.hue)
    

6. SEMICOLON
        end of expression marker.
        might be a nop.        






for i in 3{

    print(i);
}

FOR:
EXPR SHUNTING YARD:
    LOAD: i
    PUSH: 3
    IN
EXPR_END
OPEN_BLOCK
EXPR SHUNTING YARD:
    LOAD: i
    LCALL: print
    SEMICOLON
EXPR_END
CLOSE_BLOCK
END_FOR
DONE





Another way of looking at declare, and maybe others:
Arrange everything as a stack:
operands first, then operator.

Original parse:
DECLARE: number: my_num
EXPR SHUNTING YARD:
	LOAD: my_num
	LVALUE: =
	PUSH: 1
	PUSH: 2
	ADD
	ASSIGN
	SEMICOLON
EXPR_END
END DECLARE

Rewrite to:

EXPR SHUNTING YARD:
	LOAD: my_num
	LVALUE: =
	PUSH: 1
	PUSH: 2
	ADD
	ASSIGN
	SEMICOLON
EXPR_END
DECLARE: number: my_num
DONE


In this case, "number" acts as an operator which operates on the
expression "my_num = 1 + 2;"
It could also work on the simpler: "my_num;"

In this scheme, everything is pushed to the stack, including operators.


number my_num = 1 + 2;
 
1.  LOAD: my_num
        IDENT(my_num) -> TOS | IDENT(my_num)

2.  LVALUE: =
        Signals an LVALUE.
        Might be a nop.

3.  PUSH: 1
        NUMBER(1) -> TOS | NUMBER(1) IDENT(my_num)

4.  PUSH: 2
        NUMBER(2) -> TOS | NUMBER(2) NUMBER(1) IDENT(my_num)

5.  ADD
        BINOP(ADD) -> TOS | BINOP(ADD) NUMBER(2) NUMBER(1) IDENT(my_num)

6.  ASSIGN
        BINOP(ASSIGN) -> TOS | BINOP(ASSIGN) BINOP(ADD) NUMBER(2) NUMBER(1) IDENT(my_num)
        
7.  SEMICOLON
        end of expression marker.
        might be a nop.        
        
        END_EXPR -> TOS | END_EXPR BINOP(ASSIGN) BINOP(ADD) NUMBER(2) NUMBER(1) IDENT(my_num)

8.  DECLARE: number: my_num
        Create variable my_num of type number
        Check scope:
            If level 0, global, add to globals.
            If >0, local, add to current function locals.
        
        Process operations on the stack:
        Emit code for the expression and clear stack.
        
        
        



object pixelarray{ 
    number index; 
    number count; 
    bool reverse; 
};

DECLARE: object: pixelarray
OPEN_BLOCK
DECLARE: number: index
END DECLARE
DECLARE: number: count
END DECLARE
DECLARE: bool: reverse
END DECLARE
CLOSE_BLOCK
END DECLARE
DONE




Declare:
number my_num;
- type
- ident
- assign expr
- semicolon

Declare Array:
number my_num[4];
- type
- ident
- size expr (eval at compile time)
    - code
    - end expr
- assign expr
    - code
    - end expr
- semicolon

Assign:
my_num = 1 + 2;
- target (ident)
- expr
    - code
    - end expr
- semicolon

Assign Array:
my_num = [1, 2, 3, 4];
- target (ident)
- expr
    - code (push, push, push, push)
    - end expr
- semicolon

Array lookup:
my_num = my_array[1 + 2];
- target (ident)
- value (ident)
- lbracket - array subscript
- expr
    - code
    - end expr
- semicolon

Object field:
my_num = my_array.len;
- target (ident)
- value (ident)
- len (ident)
- dot

Object field 2:
my_array.len = my_num;
- target (ident)
- field (ident)
- dot
- value (ident)

Function:
fn func(number a, number b) number{};
- func name (ident)
- arg declares
- return type
- block
    - code
    - end block

Object declare:
object pixelarray{
     number index;
     number count;
     bool reverse;
};

- type (ident) pixelarray (make new type)
- open block
- field declares
    - no exprs!
- close block

For loop:
for i in 4{}
- iter (ident)
- operator (in)
- expr
    - code
    - end expr
- block
    - code
    - end block

If:
if 1 + 2{}
- expr
    - code
    - end expr
- block
    - code
    - end block

Call:
func(1 + 2, 3);
- func target (ident)
- lparen
- expr
    - code
    - end expr
- rparen



We want to be able to execute expressions *as we compile*.

That means we need to output runnable bytecode on the fly.

Basic starting point:
Declare a global var and assign to it.

number my_num = 1 + 2;

DECLARE: number: my_num
EXPR SHUNTING YARD:
EXPR_END
Output: 
    Token 32 @   7:  Ident: [my_num] Len: 6
    Token 34 @   7:  Number: [1] = 1
    Token 34 @   7:  Number: [2] = 2
    Token  1 @   7:  ADD
    Token 26 @   7:  ASSIGN
    Token 38 @   7:  SEMICOLON


This should roughly translate to:

IR:
DECLARE my_num is number
PUSH ident
PUSH number 1
PUSH number 2
ADD (number 1, number 2) from stack
ASSIGN (ident, number 3) from stack
SEMICOLON - stop token if running a single expression.


Bytecode:
PUSH 1
PUSH 2
ADD (result of 1 + 2 -> 3 is now TOS)
STORE_GLOBAL my_num


And we want to get to here:
number my_array[1 + 2];

For this to work, we need to evaluate the expression
so we can allocate memory, and we need to do it before
we get to the assign:

number my_array[1 + 2] = 3 + 4;

Within the declaration we get:
Output: 
    Token 32 @  14:  Ident: [my_array] Len: 8
    Token 20 @  14:  LBRACKET
    Token 34 @  14:  Number: [1] = 1
    Token 34 @  14:  Number: [2] = 2
    Token  1 @  14:  ADD
    Token 21 @  14:  RBRACKET
    Token 38 @  14:  SEMICOLON

IR:
PUSH ident
LBRACKET
PUSH number 1
PUSH number 2
ADD (number 1, number 2) from stack
RBRACKET
SEMICOLON - stop token if running a single expression.

For the array declare we just want to execute what is within the bracket.
However, that may not be just constants, that could be anything defined
this far.  A global var, an object (pixels.count), etc.



If we want to do this in one pass, emitting runnable bytecode as we go,
we need several things:

- Some amount of dynamic memory, especially with resizing arrays.
    - not too many, since Chromatron is limited in how many handles it can supply.
    - could work well with larger functional blocks of data (like, here's the globals, here is a function, etc)

- We need to be able to evaluate expressions during compilation.
    - We do *not* need to run arbitrary bytecode - no jumps, etc.
      We just need to run enough to evaluate expressions, so the basic 
      arithmentic stack machine, returning a single value, is fine.

- We need to be able to allocate storage for global vars as we go.
    - The aforementioned evaluation also needs to be able to read/write to global vars.
    - This includes system objects, like accessing pixels.count.

- GFX system note:
    - Chromatron supports changing pixel parameters on the fly, such as pixels.count.
    - We should consider resetting VM 0 (or any VM that accesses system parameters)
      anytime a gfx system parameter changes (in addition to reconfiguring hardware, etc).
    - This ensures scripts are synchronized with the latest system data, since
      now we can do some script startup things based on that data.

- Being able to do this should also allow us to do constant folding.

- We are building the runnable VM state as we go.

- We need an interface to run the current expression on the stack, and return
  the computed result.

- This can also be an assign.

- Assigns should "return" their assigned value (which makes it act as a true binop).

- We need interfaces to add items to VM data structures dynamically, since we may need
  to execute bytecode at any time.
    - Globals
    - Object definitions
    - Functions (hashes/pointers to code blocks)
    - Constants
    - Strings (which actually are just hashes and strings live in the KVDB!)
    - Labels (jump targets)
    - Function objects contains:
        - args (with types)
        - return type (can be none)
        - locals (which are offsets into function stack frame)

- We already have a stack and we can make that able to dynamically change size too.

- The program effectively "runs" as it goes.
    - As soon as we have a var declared and hit an expression, we can start running
      basic bytecode.

- We want to dynamically build the program state (bytecodes, var defs, etc) as we go.
    - Includes doing calls
    - Includes calling FX4 functions that are currently defined.
    - Thus, global state can be extended anywhere in the program file outside of 
      a function block.

- Ideally, when completed, we would cache the final binary image in a ready-to-start
  state.
    - We would need to clear this cache and recompile if any system parameters changed.
    - Does this actually matter?  Compilation should be pretty fast.
    - We can skip this until we prove we need it.

- Need to be able to yield so we can clean up memory.
    - Reallocs require a garbage collection run

- We want to truck around as little excess state as possible.
    - Only the bare minimum needed to *run* the program!
    - Input text can be read in small chunks from file system!

- We produce a ready to run binary image.  
    - Although we've run some code in the process of making the image, the compilation
      process ends there.  The init() and loop() functions (and sequencers) will be run
      by the main graphics timing loop.

- In theory we could allocate new program objects (functions, new globals, etc) as
  the program runs.
    - We have no actual need to do this.

- This probably limits some optimizations we can do.
    - We don't really care.

- We can probably get some dead-code-elimination in.
    - Some jump conditions are obvious, like when we know it is constant (because we folded!)
    - We probably can't prove that a variable isn't used
        - But in the context of what FX4 is used for (typically 1-2 pages max), does this
          really matter?  We generally care more about saving cycles than bytes.
    - We could do profiling if we really wanna.

- Need to figure out how to resolve jumps... especially forward jumps!
    - Forward jumps are jumping to sections of code that we haven't even 
      tokenized yet (possibly haven't even read from the file system).
    - A forward jump gets an index into the jump table.
    - Put the jump label and index into a temp label resolution structure.
    - When we encounter the label, we resolve matching jumps.
    - Possibly the jump table is u32[] and we just bite the bullet on the wasted space.
    - It inits as a hash but is overwritten by the label PC address when the label
      is emitted.  
    - We don't actually create a label opcode.
    

Given all of that:
We need a dynamic array structure.
However, we can demo with static arrays.
Interface to the program builder is through function calls.
Builder can emit a VM with runnable bytecode and data state.





Arrays:

Need to allocate storage
Same story with strings

Allocate in global space.

Actually, all objects go in global space.
Array is similar to object but indexed differently.

Don't forget we need multi-D arrays

Arrays and objects (and also functions) are all
operated on via a reference.
So the scalar value contains the pointer (or index)
in the global data pool.

Strings do store as actual strings, same as FX3.


We keep variable type info/meta in the program structure.
Hash of name, data type, array len.


Subscripting does a stride * offset calculation.
Stride is 1 word for simple arrays of scalars.
Stride and len are part of the object properties.

The only real difference is that objects are accessed 
via field name and can contain a mix of datatypes,
while arrays are accessed by index and contain the
same datatype.


Need to get normal assigns to work.
Need to handle setting up lvalue for assigns.




Subscript and field lookup compute an offset
into the item's storage.

So the item itself refers to the base pointer:

number my_num[1];

This is assigned to say, position 2 in globals and
consumes 4 words (positions 2, 3, 4, 5)

my_num supplies the base position, so that pushes
2 onto the stack.

Compute subscript expression, final result is on stack.
In this case, just a single constant: 1.

We have TOS | 1 2 on the stack.

Next we hit the subscript (one of the brackets).
This generates a lookup instruction.
Lookup takes both numbers, adds then, and places
final result on stack.

TOS | 3

Then we can access via LOAD_GLOBAL_OFFSET and
STORE_GLOBAL_OFFSET.

These take from the stack the computed offset from
above and a value that would have been pushed to the
stack beforehand.

The lookup is basically an addition, but it can
contain additional logic, for instance, arrays
in the FX language wrap around, so it can perform
a mod operation on the index and length.

Object lookups are really similar, the field
has an index into the structure, so the lookup matches
field name to index.


Strings *cannot* be indexed.



number my_num = 5;

Token 28 @  13:  DECLARE
Token 32 @  13:  Ident: [my_num] Len: 6
Token 26 @  13:  ASSIGN
Token 34 @  13:  Number: [5] = 5
Token 38 @  13:  SEMICOLON
Token  0 @  35:  END


number my_num;
my_num = 5;

Token 32 @  12:  Ident: [my_num] Len: 6
Token 33 @  12:  LVALUE
Token 34 @  12:  Number: [5] = 5
Token 26 @  12:  ASSIGN
Token 38 @  12:  SEMICOLON

We need these patterns to match.




Ok now it is:

number my_num = 5;

Token 32 @  13:  Ident: [my_num] Len: 6
Token 33 @  13:  LVALUE
Token 34 @  13:  Number: [5] = 5
Token 26 @  13:  ASSIGN
Token 38 @  13:  SEMICOLON


number my_num;
my_num = 5;

Token 32 @  12:  Ident: [my_num] Len: 6
Token 33 @  12:  LVALUE
Token 34 @  12:  Number: [5] = 5
Token 26 @  12:  ASSIGN
Token 38 @  12:  SEMICOLON


number my_array[4];

Token 32 @  22:  Ident: [my_array] Len: 8
Token 20 @  22:  LBRACKET
Token 34 @  22:  Number: [4] = 4
Token 21 @  22:  RBRACKET
Token 38 @  22:  SEMICOLON



number my_array[4 + 1];

Token 32 @  22:  Ident: [my_array] Len: 8
Token 20 @  22:  LBRACKET
Token 34 @  22:  Number: [4] = 4
Token 34 @  22:  Number: [1] = 1
Token  1 @  22:  ADD
Token 21 @  22:  RBRACKET
Token 38 @  22:  SEMICOLON


number my_num = my_array[1];

Token 32 @  22:  Ident: [my_num] Len: 6
Token 33 @  22:  LVALUE
Token 32 @  22:  Ident: [my_array] Len: 8
Token 20 @  22:  LBRACKET
Token 34 @  22:  Number: [1] = 1
Token 21 @  22:  RBRACKET
Token 26 @  22:  ASSIGN
Token 38 @  22:  SEMICOLON


What if we have an IR?
The IR is itself a stack machine.
IT executes more complex instructions
that operate on compiler internals.

This naturally encodes from the
already stack friendly parser
output.

It strips the token context that
we no longer need.
Because parameters contain type information,
operators can do different things
depending on type.


IR:
written as function calls -
push args on stack, then push
operator to use.
-> is return value pushed to stack

DECLARE ident, type -> ident
    declare returns with ident on the stack

OBJ FIELD ident, type
    push to sack

OBJ DEFINE ident, arg_count
    pop arg_count fields from stack
    create object definition

OBJ_DECLARE ident, obj_type
    

ASSIGN target, value
    handles if target, value are refs, etc

BINOP left, right, op -> value
    perform binary arithmetic

CONSTANT value
    add value to constant table

FUNC_ARGS ident, type
FUNC_DEFINE ident, arg_count, return_type

IF value
    jump to end of block if false
    fall through if true
ELSE
    insert jump to end of block
    to catch fall through from IF.
    initial jump if false should land here

FOR iter, condition expr
    open block
    if expr == 0:
        jump to end of loop

    local var iter
    set iter to 0

    label

    do loop body

    iter++

    if iter < expr:
        jump to label
    else:
        fall through, loop is done
    
    end of loop label

    close block

WHILE condition expr
    open block

    label
    
    if expr == 0:
        jump to end of loop
    
    do loop body

    jump to label

    end of loop label
    
    close block



We can use the IR generation to
compute the amount of memory we need
for various data structures.
Actually.. we can't, because
we haven't *run* expressions yet.
But we can run them in the IR easily.

Don't need IF/FOR/etc
We can do LOOP, JUMP, LABEL, etc.




FX4 obviates the need for a second simulator.
There is only the one VM in C.



Biggest problem right now is we need to run expressions
as we go to compute memory allocations.
Array sizes can vary and need the stack machine to run.

What if we translate to a stack based IR first?

What can we get in the first pass?

Pass 1: Conversion to stack IR
We get the types and names of all vars.
We get expression IR code for array sizes.

We can't do actual memory allocation, other than
setting up the symbol table.  We will know everything
that needs to exist, just not the sizes and addresses.

What about control flow?

Since we are doing recursive descent into a queue,
we are already flattening the tree.
We can do label instructions easily and resolve
addresses in the next pass.

We don't do any type checking here, we just
record what types are being used.

The constant table is generated in this stage.


Pass 2: Run IR

IR executes.
Declares will set up actual var allocations in an
expandable array.
Globals get their own address space.

Since we have the entire symbol table available before
we start generating executable code, we can technically access
variables from anywhere, even from before they are declared,
as long as they all get their own non-overlapping storage.

Globals are accessible from anywhere, so you can init an array
size from a variable declared *below* the array.  Whoa.
Globals are a 16 bit address space.

Function locals will go in a frame possibly on the
data stack and have their own access instructions
and address space.
Locals are an 8 bit address space.
Scope is function wide, so you can declare anywhere
and the variable is available at the top.
In the interests of simplicity, we will skip doing
liveness analysis, and just allocate everything.

In general we write these scripts to be lean on memory
and we aren't really wasting variables.

This also ensures memory usage is always worst/max case 
and is fixed.  Finally, this doesn't reduce execution speed,
it just wastes a nominally trivial amount of memory in exchange
for great simplicity.


We do type checking during code gen in this pass.
We can match available instructions by operand types.

Objects are global only (this is an acceptable limitation for now).
User objects (coded in FX) share the main global space.
Dot operators work to compute a global offset, similar
to the array subscriptor.

System objects are accessed via a different bytecode
instruction and reference into a machine level C API
and can directly interact with SapphireOS. 

Since we compile on device, we can check that all system
object and function references actually exist,
and can bind to them via a table with function pointers.
This should hopefully be faster than a switch.

This is where we do dynamic arrays.

- globals
- data stack
- constants
- function meta
- bytecode

There are still declare IR instructions.  These link to 
expressions to fill in array lengths.  For scalars,
they just setup the address for that variable.
Thus, they are stored in global space in top down order.

We can do some basic optimizations during instruction
generation.
Constant folding is pretty simple to do.
We can do some multiple dispatch instructions like
push2,3,4 operands, etc.
instructions to push constants 0 and 1 and maybe 65535,
those are all commonly used on FX systems.

This is also where we put in automated type conversions.

If we have dedicated instructions for some items that are
coded as function calls (like rand possibly), we do that
in this pass as well.

We can probably do basic dead code elimination.

There is no register allocation at all, courtesy of 
the stack machine.

Jumps can usually use an 8 bit offset, but can offer an
alternate or trampoline if we need more.

Since we have type checked, we can throw away type information
if we don't have a need for it.
We can deallocate some of the no longer needed meta data.


The system should be constructed such that it is
possible to suspend/resume at anytime, at any instruction.
Thus, we should not need to set up additional state
to suspend.
The only data that goes on the C stack in the bytecode
loop is temporaries for the current instruction.
Between instructions, nothing relies on state from the 
C stack.


IR is thrown away after code generation is complete.


"Pointers" in the FX VM are 16 bit integers pointing
to an implicit table.


We optimize instructions in the IR phase, so once we have
the bytecode sequence, it is complete.

In general, I think we only need to grow dynamic arrays
and delete them, never truncate.



Another option is generate IR straightaway from parser.
As simple as possible.
Then we can through IR twice:

First pass sets up symbol tables and does type checking/annotation.
Second pass does instruction generation.

Actually, maybe move type checking to second pass.
So the first pass is just symbols and annotation.

So this is 3 "passes" total:
Parser
    - build IR stack

IR Pass 1:
    (no bytecode in this pass)
    - symbol tables
        - might scan the array twice, first to get count and allocate,
          second to fill array
    - type annotation
    - jump table setup

IR Pass 2:
    (bytecode runs during this pass)
    (this pass sets up a runnable machine as it goes)
    - type checking
    - instruction selection and generation
    - memory allocation
    - set jump targets
    - constant folding
    - dead code elimination
    - redundant jump elimination (jumps to next instruction)

Probably some cleanup/final output code

Type checking:
Instruction selection for IR operations is based on
the types of the inputs to that instruction.
For instance binop takes left and right values and an
operator.
There may be different instructions for the operator
based on the types of the inputs.
If there is no matching instruction, that is probably
a type error.
Here we can also add code to do implicit conversions.
The only required conversion is i32 to f16 and vice 
versa.  

DCE: can skip rendering obviously dead code segments.

For instance:

if 0{
    
    nothing here will run.
    we know the jump to the end of the block
    is always taken.
}


Any bytecode that has a constant input can be
evaluated immediately.









Dynamically allocate memory at run time?
For arrays/etc.  
Create/modify array would be an opcode.

It is generally undecidable how much memory our
new design uses at compile time.

The inclusion of dynamically sized arrays guarantees that.

Thus, if we want that kind of language feature, we lose
the ability to predict memory usage in advance on arbitrary
input.

*Since* we don't have that, then we can just do dynamically
allocated arrays for core VM memory regions and the 
DECLARE token/IR operation directly translates to a bytecode 
instruction.

Declares will still build a symbol table with the type info.
Scalar vars can be statically allocated up front, this will
help with speed.

Scalar access to arrays use the LVALUE opcode to lookup
the address.

This means we can just run code and the physical memory
of the VM is constructed as we go.

The instruction list, constant table, global symbol table,
function list, scalar global vars, and others are static at 
compile time.

The machine stack itself, which can also house local variables
(if we have the correct offset instructions, not just pop),
can also be dynamically sized.


SapphireOS has an excellent memory manager that can handle this well.

Since we are building this capability into the language,
dynamic data structures become possible in the future.
















L and R values

Need to distinguish L and R values for IDENTS
so we can select load and store instructions.
This is easier in tree form but what we have is a stack.

LVALUE:

DECLARE: number: my_num
EXPR SHUNTING YARD:
Token 32 @   3:  Ident: [my_num] Len: 6
Token 33 @   3:  LVALUE
Token 34 @   3:  Number: [3] = 3
Token 26 @   3:  ASSIGN
Token 38 @   3:  SEMICOLON
EXPR_END
DONE with parsing
IR: DECLARE = my_num type: 1 @ 3
IR: PUSH_IDENT = my_num @ 3
IR: LVALUE @ 3
IR: PUSH_CONST = 3 @ 3
IR: BINOP ASSIGN @ 3
IR: END_EXPR @ 3


Here the tree would be:

    assign
    /    \
my_num    3

store to my_num

stack version:

my_num
3
assign



RVALUE:

DECLARE: number: my_num2
EXPR SHUNTING YARD:
Token 32 @   5:  Ident: [my_num2] Len: 7
Token 33 @   5:  LVALUE
Token 32 @   5:  Ident: [my_num] Len: 6
Token 26 @   5:  ASSIGN
Token 38 @   5:  SEMICOLON
EXPR_END
DONE with parsing
IR: DECLARE = my_num2 type: 1 @ 5
IR: PUSH_IDENT = my_num2 @ 5
IR: LVALUE @ 5
IR: PUSH_IDENT = my_num @ 5
IR: BINOP ASSIGN @ 5
IR: END_EXPR @ 5



Here the tree would be:

    assign
    /    \
my_num2   my_num

need to load my_num
then store to my_num2

stack version:

my_num2
my_num
assign


On the left side of assign, we do a store.
However, all other usages in expressions will be a load.

Can we attach the IDENT to the LVALUE?

Actually, just getting to an IR_LOAD and IR_STORE would be
preferable.






-> syntax for array mapping

map function onto array

pixels.hue -> zero()

zero(pixels.hue)
{
    for(i in array.count):
        pixels[i].hue = 0.0
}


can be chained:

pixels.hue -> sin() -> cos();

this is basically:
cos(sin(pixels.hue))


can take parameters

pixels.hue -> assign(0.5)
# sets given array to 0.5

this operator is used for array init as well:
pixels.hue -> (0, 1, 2, 3)

we do not do:
pixels.hue = [0, 2, 3, 4]
# or somesuch



Declaration: compute array size and create array
IR: STORE = my_array @ 2
IR: LBRACKET @ 2
IR: PUSH_CONST = 4 @ 2
IR: RBRACKET @ 2
IR: LBRACKET @ 2
IR: PUSH_CONST = 3 @ 2
IR: RBRACKET @ 2
IR: END_EXPR @ 2
IR: ARRAY_SIZE @ 2

Store to array at index
Load will be similar
IR: STORE = my_array @ 4
    push array address/index on the stack
IR: LBRACKET @ 4
    push stride at this dim?)

IR: PUSH_CONST = 1 @ 4
    
IR: RBRACKET @ 4
    pop index
    pop stride
    push index * stride

IR: LBRACKET @ 4
    push stride at this dim?

IR: PUSH_CONST = 2 @ 4


IR: RBRACKET @ 4
IR: PUSH_CONST = 99 @ 4
IR: BINOP ASSIGN @ 4
IR: END_EXPR @ 4


What if we did indexing syntax slightly differently?

Instead of:
array[1][2][3]

How about:
array[1, 2, 3]

This is how we would actually write this in math.

We want to support true multi_d arrays.


STORE ARRAY
    push index in globals of this variable
    this contains meta information

LBRACKET
    push stride of array
    (parser should know type/size)

CONST
    push const

COMMA
 (MUL)
    pop a
    pop b
    push a * b

CONST
    push const
    (second dimension)

COMMA
 (MUL)
    pop a
    pop b
    push a * b

CONST
    push const
    (third dimension)

RBRACKET
    pop a
    temp_index = a

CONST
    push const
    (value to assign)
    (this could be an expression)

BINOP_ASSIGN
    pop value
    pop index
    pop meta index

    arrays[meta.offset + index] = value

In the declare path, instead of binop assign we would have
array size.

ARRAY_SIZE:
    pop value
    pop index
    pop meta index
    
    write meta?
    allocate actual array



Need to compute strides for each dimension






saturated math:
$-
$+
$/
$*
$%





Functions/Coroutines

1. Re-entrancy and Recursion:

Do we need this in the general case?

We don't have pre-emption and recursion 
eats memory (unless we can do tail calls, but not 
all uses of recursion can).
We don't want to waste effort on TCO.

If functions are not re-entrant then they
do not need function local storage.
Global storage is easy, just prefix function name
to avoid name conflicts.

However - how do we detect recursive calls?
We can at run time by scanning the call stack, 
at cost to execution time.

And if we don't detect them, then recursion can blow
the stack and possibly crash the machine.
Ideally we aren't checking stack depth since we 
would have to check on every instruction for overflow,
at great runtime cost.

For statically called functions, the compiler can
just check this as it descends the calls.
But for indirect calls, we don't know until
runtime.

We could have some bookkeeping on called functions, like
a bitmask, memory efficient.  Check if bit is set on call,
set if not, error out if set.
Clear bit on return.
This adds more overhead to every call/return.

Is this a real problem?
At some point you just do need to learn to code
properly.  Since function re-entrancy is a property
of how a given function is coded, you can fuck
this up in a lot of languages.



2. Coroutines:

In FX3 these are "threads", a poor naming choice.

We basically want little "render loops".
delay/yield, etc.

They can be scheduled by millisecond timer,
but all will run on the frame tick in a loop
until all timers are waiting for the next frame.

These are a function, but get some kind of local storage
that supports suspend/resume at the top level.  Cannot 
be on the stack, since we can't s/r without moving the
entire stack around.  We will only support s/r at top level
so functions called from a coroutine cannot do s/r.
Also cannot call a coroutine from anywhere, can only
be run by the scheduler.

These will have their own keyword to declare.



Decisions for now:

Functions are not re-entrant and function locals are 
name prefixed globals.  Easy to implement and debug.

Coroutines will get their own storage mechanism.







Problems:

Array index does not have number of expressions, it relies
on rank.  But if you give it the wrong number of indexes
the VM will just break.  No error detection.


Object lookup:
This is just clunky in the shunting yard output.

We have to resolve indexes first on both sides of the
dot before running the dot.


Our ideal system object API is something like this:

obj_load(obj_hash, obj_index, field_hash, field_index, *value)
obj_store(obj_hash, obj_index, field_hash, field_index, value)

But the current IR output will reduce everything down
to 2 indexed offsets on the stack for the dot to consume.






First class DECLARE in bytecode?

Could this be easier than trying to balance symbol creation/allocation
at compile time and needing to evaluate array sizes at run time?

Just allocate everything at runtime.


Declare opcode:
params: array rank



Symbol table:

ident hash, type, rank, function_index


rank = 0 is scalar
rank > 0 is array of whatever that type is

Types:

Scalar:
number, bool, fixed16, float

Variable length:
string
array
object
function


Objects get a secondary lookup:


Object table:

object hash, field hash, rank, type


It would be nice to actually store everything in one contiguous dynamic array
Globals, locals, and instructions.
Everything is referenced by 16 bit offset into VM memory.




When we do load/store, what do we *mean*?

For scalars, it is simple:

Load = load value from globals of that item to stack
Store = store value in globals of that item from stack


For dynamics:

All dynamics start as a "scalar" reference value in globals.

When we load, we are loading that "value", which actually contains
reference information.
Downstream instructions need to know what to do with that information.


Intermediate Machine:

What if we build an intermediate machine first?

This uses the same bytecode machine, but with slightly different instructions.



DECLARE:
pop ident hash from stack
pull type from operands
pull rank from operands

Create symbol table entry

IDENT:
push ident hash to stack (via constants), so a PUSH_CONST



 */








