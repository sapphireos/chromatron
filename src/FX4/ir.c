
#ifdef FX4_COMPILER

#include <stdio.h>
#include <string.h>

#include "ir.h"
#include "program.h"
#include "hash32.h"
#include "bytecode.h"
#include "array.h"
#include "syslib.h"
// #include "pixelarray.h"
#include "debug_printf.h"

char* get_str_for_type(ir_type_t type){

	switch(type){
		case IR_NOP:
			return "NOP";

		case IR_END:
			return "END";

		case IR_START:
			return "START";

		case IR_DECLARE:
			return "DECLARE";

		case IR_ASSERT:
			return "ASSERT";

		case IR_ASSIGN:
			return "ASSIGN";

		case IR_BINOP:
			return "BINOP";

		case IR_ASSIGN_OP:
			return "ASSIGN_OP";

		case IR_LVALUE:
			return "LVALUE";

		case IR_UNARYOP:
			return "UNARYOP";

		case IR_PUSH_CONST:
			return "PUSH_CONST";

		case IR_HALT:
			return "HALT";

		case IR_END_EXPR:
			return "END_EXPR";

		case IR_FUNC:
			return "FUNC";

		case IR_END_FUNC:
			return "END_FUNC";

		case IR_CALL:
			return "CALL";

		case IR_JUMP:
			return "JUMP";

		case IR_LABEL:
			return "LABEL";

		case IR_RETURN:
			return "RETURN";

		case IR_YIELD:
			return "YIELD";

		case IR_DELAY:
			return "DELAY";

		case IR_RATE:
			return "RATE";

		case IR_PUBLISH:
			return "PUBLISH";

		case IR_IDENT:
			return "IDENT";

		case IR_OBJ_FIELD:
			return "OBJ_FIELD";

		case IR_OBJ_LOAD:
			return "OBJ_LOAD";

		case IR_OBJ_STORE:
			return "OBJ_STORE";

		case IR_BRACKET:
			return "BRACKET";

		case IR_DOT:
			return "DOT";

		case IR_LOOP_INIT:
			return "LOOP_INIT";

		case IR_LOOP_EXIT:
			return "LOOP_EXIT";

		default:
			break;
	}

	return "unknown";
}

char* get_str_for_operator(ir_operator_t op){

	switch(op){
		case IR_OP_ADD:
			return "ADD";

		case IR_OP_SUB:
			return "SUB";

		case IR_OP_MUL:
			return "MUL";

		case IR_OP_DIV:
			return "DIV";

		case IR_OP_MOD:
			return "MOD";

		case IR_OP_IN:
			return "IN";

		case IR_OP_COMPEQ:
			return "COMPEQ";
		
		case IR_OP_COMPNEQ:
			return "COMPNEQ";

		case IR_OP_COMPLT:
			return "COMPLT";

		case IR_OP_COMPLTE:
			return "COMPLTE";

		case IR_OP_COMPGT:
			return "COMPGT";

		case IR_OP_COMPGTE:
			return "COMPGTE";

		default:
			break;
	}

	return "unknown";
}

void ir_init(ir_state_t *ir_state){

	ir_state->ir_list 			= array_alloc(sizeof(ir_t), 64);
	ir_state->opcode_list 		= array_alloc(sizeof(uint8_t), 64);
	ir_state->const_list 		= array_alloc(sizeof(int32_t), 8);
	ir_state->jump_targets 		= array_alloc(sizeof(jump_target_t), 8);
	ir_state->global_symbols 	= array_alloc(sizeof(var_info_t), 16);
	ir_state->functions 		= array_alloc(sizeof(func_def_t), 4);
	ir_state->objects 			= array_alloc(sizeof(obj_def_t), 2);

	#ifdef IR_ENABLE_DEBUG_LISTING
	ir_state->debug_listing 	= array_alloc(sizeof(ir_debug_listing_t), 2);
	#endif

	ir_state->func_index = -1;
}

void ir_deinit(ir_state_t *ir_state){

	array_free(ir_state->ir_list);
	array_free(ir_state->opcode_list);
	array_free(ir_state->const_list);
	array_free(ir_state->jump_targets);
	array_free(ir_state->global_symbols);
	array_free(ir_state->functions);
	array_free(ir_state->objects);

	#ifdef IR_ENABLE_DEBUG_LISTING
	array_free(ir_state->debug_listing);
	#endif
}

int ir_get_state_size(ir_state_t *ir_state){

	return array_get_used_space(ir_state->ir_list) + 
		   array_get_used_space(ir_state->opcode_list) +
		   array_get_used_space(ir_state->const_list) +
		   array_get_used_space(ir_state->jump_targets) +
		   array_get_used_space(ir_state->global_symbols) +
		   array_get_used_space(ir_state->functions) +
		   array_get_used_space(ir_state->objects) +
		   #ifdef IR_ENABLE_DEBUG_LISTING
		   array_get_used_space(ir_state->debug_listing) +
		   #endif
		   sizeof(ir_state_t);

}

void ir_add(ir_state_t *ir_state, const ir_t *ir){

	array_add(&ir_state->ir_list, ir);
}

int ir_count(ir_state_t *ir_state){

	return array_get_count(ir_state->ir_list);
}

ir_t* ir_get_index(ir_state_t *ir_state, int index){

	if(index >= array_get_count(ir_state->ir_list)){

		return NULL;
	}

	ir_t *ptr = (ir_t *)array_get_data(ir_state->ir_list);

	if(index < 0){

		index += array_get_count(ir_state->ir_list);
	}

	return &ptr[index];
}

void ir_peek(ir_state_t *ir_state, ir_t *ir){

	int count = array_get_count(ir_state->ir_list);

	if(count < 1){

		memset(ir, 0, sizeof(ir_t));
		return;
	}

	ir_t *ir_ptr = (ir_t *)array_get_data(ir_state->ir_list);

	*ir = ir_ptr[count - 1];
}

void ir_print(const ir_t *ir){

	if(ir->type == IR_BINOP){

		debug_printf("IR: %s %s @ %d\n", get_str_for_type(ir->type), get_str_for_operator(ir->args.binop.op), ir->line_num);
	}
	else if(ir->type == IR_PUSH_CONST){

		debug_printf("IR: %s = %d @ %d\n", get_str_for_type(ir->type), ir->args.constant.value, ir->line_num);	
	}
	else if(ir->type == IR_IDENT){

		debug_printf("IR: %s = %s @ %d\n", get_str_for_type(ir->type), hash_s_get_str(ir->args.ident.ident_hash), ir->line_num);	
	}
	else if(ir->type == IR_LVALUE){

		debug_printf("IR: %s = %s @ %d\n", get_str_for_type(ir->type), hash_s_get_str(ir->args.ident.ident_hash), ir->line_num);	
	}
	else if(ir->type == IR_DECLARE){

		debug_printf("IR: %s = %s type: %d @ %d\n", get_str_for_type(ir->type), hash_s_get_str(ir->args.declare.ident_hash), ir->data_type, ir->line_num);	
	}
	else if(ir->type == IR_JUMP){

		if(ir->args.jump.jump == IR_JUMP_ALWAYS){

			debug_printf("IR: %s -> %d @ %d\n", "JUMP", ir->args.jump.label_index, ir->line_num);	
		}
		else if(ir->args.jump.jump == IR_JUMP_ZERO){

			debug_printf("IR: %s -> %d @ %d\n", "JUMPZ", ir->args.jump.label_index, ir->line_num);	
		}
		else if(ir->args.jump.jump == IR_JUMP_NOT_ZERO){

			debug_printf("IR: %s -> %d @ %d\n", "JUMPNZ", ir->args.jump.label_index, ir->line_num);	
		}
		else if(ir->args.jump.jump == IR_JUMP_BREAK){

			debug_printf("IR: %s -> %d @ %d\n", "BREAK", ir->args.jump.label_index, ir->line_num);	
		}
		else if(ir->args.jump.jump == IR_JUMP_CONTINUE){

			debug_printf("IR: %s -> %d @ %d\n", "CONTINUE", ir->args.jump.label_index, ir->line_num);	
		}
		else{

			debug_printf("IR: %s -> %d @ %d\n", "JUMP ???", ir->args.jump.label_index, ir->line_num);	
		}
	}
	else if(ir->type == IR_LABEL){

		debug_printf("IR: LABEL <- %d @ %d\n", ir->args.label.label_index, ir->line_num);	
	}
	else{

		debug_printf("IR: %s @ %d\n", get_str_for_type(ir->type), ir->line_num);	
	}
	
}

void ir_print_list(array_t ir_list){

	ir_t *ir = array_get_data(ir_list);

	for(int i = 0; i < array_get_count(ir_list); i++){

		ir_print(ir);
		ir++;
	}
}

void ir_print_symbols(ir_state_t *ir_state){

	debug_printf("IR Symbol table:\n");

	int count = 0;

	var_info_t *var_info = array_get_data(ir_state->global_symbols);
	func_def_t *func_def = array_get_data(ir_state->functions);

	for(int i = 0; i < array_get_count(ir_state->global_symbols); i++){

		char* func_name = "global";

		if(var_info->func_index >= 0){

			func_name = hash_s_get_str(func_def[var_info->func_index].name_hash);
		}

		debug_printf("\t%3d: %16s func: %16s type: %d\n", i, hash_s_get_str(var_info->name_hash), func_name, var_info->type);

		var_info++;
		count++;
	}	

	debug_printf("\t%d symbols\n", count);
}

void ir_print_objects(ir_state_t *ir_state){

	debug_printf("IR Object table:\n");

	obj_def_t *obj_def = array_get_data(ir_state->objects);

	for(int i = 0; i < array_get_count(ir_state->objects); i++){

		debug_printf("\t%s = \n", hash_s_get_str(obj_def[i].name_hash));

		for(int j = 0; j < obj_def[i].field_count; j++){

			debug_printf("\t\t%s: %d\n", hash_s_get_str(obj_def[i].fields[j].name_hash), obj_def[i].fields[j].type);
		}
	}	

	debug_printf("\t%d objects\n", array_get_count(ir_state->objects));
}

void ir_print_functions(ir_state_t *ir_state){

	debug_printf("IR Function table:\n");

	func_def_t *func_def = array_get_data(ir_state->functions);

	const jump_target_t *jump_targets = (jump_target_t *)array_get_data(ir_state->jump_targets);

	for(int i = 0; i < array_get_count(ir_state->functions); i++){

		debug_printf("\t%s label: %d ins offset: %d jump target: %d locals: %d\n", 
			hash_s_get_str(func_def[i].name_hash), 
			func_def[i].label_index, 
			func_def[i].instruction_offset, 
			jump_targets[func_def[i].label_index].instruction_offset,
			func_def[i].locals_index);
	}	

	debug_printf("\t%d functions\n", array_get_count(ir_state->functions));
}

int ir_type_size(data_type_t data_type){

	if(data_type == TYPE_NONE){

		return 0;
	}

	return 1;
}

int ir_obj_size(const obj_def_t *obj_def){

	int size = 0;

	for(int i = 0; i < MAX_OBJECT_FIELDS; i++){

		size += ir_type_size(obj_def->fields[i].type);
	}

	return size;
}

int ir_lookup_obj_def(ir_state_t *ir_state, uint32_t obj_hash, obj_def_t *obj_def){

	obj_def_t *obj_ptr = array_get_data(ir_state->objects);

	for(int i = 0; i < array_get_count(ir_state->objects); i++){

		if(obj_ptr[i].name_hash == obj_hash){

			if(obj_def != NULL){

				*obj_def = obj_ptr[i];
			}

			return i;
		}
	}

	// not found
	return -1;
}

int ir_lookup_obj_field(ir_state_t *ir_state, uint32_t obj_hash, uint32_t field_hash, obj_def_t *obj_def){

	var_info_t var_info = {0};	

	if(ir_lookup_symbol(ir_state, obj_hash, &var_info) < 0){

		return -1;
	}

	if(var_info.obj_index >= array_get_count(ir_state->objects)){

		printf("Invalid obj index\n");

		return -1;
	}

	obj_def_t *obj_ptr = array_get_data(ir_state->objects);
	const obj_def_t *temp_obj_def = &obj_ptr[var_info.obj_index];

	// match!
	if(obj_def != NULL){

		*obj_def = *temp_obj_def;
	}

	if(field_hash == 0){

		return 0;
	}

	// search for fields
	for(int i = 0; i < MAX_OBJECT_FIELDS; i++){

		if(temp_obj_def->fields[i].name_hash == HASH32(_any)){

			return i;
		}
		else if(temp_obj_def->fields[i].name_hash == field_hash){

			return i;
		}
	}	

	return -1;
}


static int ir_search_global(ir_state_t *ir_state, uint32_t hash){

	var_info_t *ptr = array_get_data(ir_state->global_symbols);

	// search local
	for(int i = 0; i < array_get_count(ir_state->global_symbols); i++){

        if((ptr->name_hash == hash) && (ptr->func_index < 0)){

            return i;
        }

        ptr++;
    }	

    return -1;
}

static int ir_search_local(ir_state_t *ir_state, uint32_t hash){

	var_info_t *ptr = array_get_data(ir_state->global_symbols);

	// search local
	for(int i = 0; i < array_get_count(ir_state->global_symbols); i++){

        if((ptr->name_hash == hash) && (ptr->func_index == ir_state->func_index)){

            return i;
        }

        ptr++;
    }	

    return -1;
}

int ir_lookup_symbol(ir_state_t *ir_state, uint32_t hash, var_info_t *var_info){

	int index = ir_search_global(ir_state, hash);

	if(index < 0){

		index = ir_search_local(ir_state, hash);
	}

	if(index < 0){

		return -1;
	}

	if(var_info != NULL){

		var_info_t *ptr = array_get_data(ir_state->global_symbols);

		*var_info = ptr[index];
	}

	return index;
}

int ir_lookup_symbol_ptr(ir_state_t *ir_state, uint32_t hash, var_info_t **var_info){

	int index = ir_search_global(ir_state, hash);

	if(index < 0){

		index = ir_search_local(ir_state, hash);
	}

	if(index < 0){

		return -1;
	}

	if(var_info != NULL){

		var_info_t *ptr = array_get_data(ir_state->global_symbols);

		*var_info = &ptr[index];
	}

	return index;
}

int ir_lookup_type(ir_state_t *ir_state, uint32_t hash, data_type_t *data_type){

	var_info_t info;

	int status = ir_lookup_symbol(ir_state, hash, &info);

	if(status < 0){

		return -1;
	}

	*data_type = info.type;

	return 0;
}


int ir_add_global_var(ir_state_t *ir_state, uint32_t name_hash, data_type_t type){

    var_info_t var_info = {
        type,
        false, // is ref
        name_hash,
        0,
        0,
        -1, // func index
        -1, // addr
    };

    array_add(&ir_state->global_symbols, &var_info);

    return 0;
}

int ir_add_local_var(ir_state_t *ir_state, uint32_t name_hash, data_type_t type, int8_t func_index){

    var_info_t var_info = {
        type,
        false, // is ref
        name_hash,
        0,
        0, 
        func_index,
        -1, // addr
    };

    array_add(&ir_state->global_symbols, &var_info);

    return 0;
}

int ir_add_object(ir_state_t *ir_state, uint32_t obj_hash){

	int index = ir_lookup_obj_def(ir_state, obj_hash, NULL);

	if(index >= 0){

		printf("Object: %s already exists!\n", hash_s_get_str(obj_hash));
	}

	// add object
    obj_def_t obj_def = {
        obj_hash,
        0,
        {},
    };

	array_add(&ir_state->objects, &obj_def);

	return array_get_count(ir_state->objects) - 1;
}

int ir_add_object_field(ir_state_t *ir_state, uint32_t obj_hash, uint32_t field_hash, data_type_t data_type){

	ir_t ir_obj_field = {
        .type = IR_OBJ_FIELD,
        .line_num = -1,
        .args.obj = {
            .obj_hash = obj_hash,
            .data_type = data_type,
            .field_hash = field_hash,
        },
        .data_type = TYPE_OBJECT,
    };

    ir_add(ir_state, &ir_obj_field);

    return 0;
}

int ir_declare_object(ir_state_t *ir_state, uint32_t type_hash, uint32_t name_hash){

	int obj_idx = ir_lookup_obj_def(ir_state, type_hash, NULL);

	if(obj_idx < 0){

		return -1;
	}

	ir_t ir = {
        .type = IR_DECLARE,
        .line_num = -1,
        .args.declare = {
            .ident_hash = name_hash,
        },
        .data_type = TYPE_OBJECT | obj_idx,
    };

    ir_add(ir_state, &ir);

	return 0;
}


int ir_lookup_libfunc(ir_state_t *ir_state, uint32_t func_hash, func_def_t *func_def){

	return syslib_lookup_func_def(func_hash, func_def);	
}

int ir_lookup_func(ir_state_t *ir_state, uint32_t func_hash, func_def_t *func_def){

	const func_def_t *func = array_get_data(ir_state->functions);

	for(int i = 0; i < array_get_count(ir_state->functions); i++){

		if(func[i].name_hash == func_hash){

			if(func_def != NULL){

				*func_def = func[i];
			}

			return i;
		}
	}

	return -1;
}

int ir_lookup_func_by_index(ir_state_t *ir_state, int index, func_def_t *func_def){

	if(index < 0){

		return -1;
	}

	if(index >= array_get_count(ir_state->functions)){

		return -1;
	}

	const func_def_t *func = array_get_data(ir_state->functions);

	if(func_def != NULL){

		*func_def = func[index];
	}

	return index;
}

int ir_add_func_local(ir_state_t *ir_state, int index){

	if(index < 0){

		return -1;
	}

	if(index >= array_get_count(ir_state->functions)){

		return -1;
	}

	func_def_t *func = array_get_data(ir_state->functions);

	func[index].locals_size++;

	return 0;
}

// returns positive label
// returns negative on error
int ir_create_label(ir_state_t *ir_state){

	// check if out of labels
	if(ir_state->next_label >= IR_MAX_LABELS){

		printf("Label limit exceeded!\n");

		return -1;
	}

	return ir_state->next_label++;
}

int ir_push_entry_label(ir_state_t *ir_state, uint16_t label){

	// check if out of stack space
	if(ir_state->label_entry_index >= cnt_of_array(ir_state->label_entry_stack)){

		printf("Entry label limit exceeded!\n");

		return -1;
	}

	ir_state->label_entry_stack[ir_state->label_entry_index] = label;
	ir_state->label_entry_index++;

	return 0;
}

int ir_peek_entry_label(ir_state_t *ir_state){

	if(ir_state->label_entry_index == 0){

		printf("No entry labels!\n");

		return -1;
	}

	return ir_state->label_entry_stack[ir_state->label_entry_index - 1];
}

int ir_pop_entry_label(ir_state_t *ir_state){

	if(ir_state->label_entry_index == 0){

		printf("No entry labels!\n");

		return -1;
	}

	ir_state->label_entry_index--;

	return 0;
}

int ir_push_exit_label(ir_state_t *ir_state, uint16_t label){

	// check if out of stack space
	if(ir_state->label_exit_index >= cnt_of_array(ir_state->label_exit_stack)){

		printf("Entry label limit exceeded!\n");

		return -1;
	}

	ir_state->label_exit_stack[ir_state->label_exit_index] = label;
	ir_state->label_exit_index++;

	return 0;
}


int ir_peek_exit_label(ir_state_t *ir_state){

	if(ir_state->label_exit_index == 0){

		printf("No exit labels!\n");

		return -1;
	}

	return ir_state->label_exit_stack[ir_state->label_exit_index - 1];
}

int ir_pop_exit_label(ir_state_t *ir_state){

	if(ir_state->label_exit_index == 0){

		printf("No exit labels!\n");

		return -1;
	}

	ir_state->label_exit_index--;

	return 0;
}

int ir_push_stack(ir_state_t *ir_state, const ir_t *ir){

	if(ir_state->stack_index >= cnt_of_array(ir_state->ir_stack)){

		printf("IR stack overflow\n");
		return -1;
	}

	const ir_t *ir_array = array_get_data(ir_state->ir_list);

	// compute index
	uint16_t index = ir - ir_array;

	// push to stack
	ir_state->ir_stack[ir_state->stack_index++] = index;

	return 0;
}

int ir_pop_stack(ir_state_t *ir_state, ir_t *ir){

	if(ir_state->stack_index == 0){

		printf("IR stack underflow\n");
		return -1;
	}

	--ir_state->stack_index;

	uint16_t index = ir_state->ir_stack[ir_state->stack_index];

	if(ir != NULL){

		const ir_t *ir_array = array_get_data(ir_state->ir_list);

		*ir = ir_array[index];
	}
	
	return index;
}

int ir_peek_stack(ir_state_t *ir_state, ir_t *ir){

	if(ir_state->stack_index == 0){

		printf("IR stack underflow\n");
		return -1;
	}

	uint16_t index = ir_state->ir_stack[ir_state->stack_index - 1];

	if(ir != NULL){

		const ir_t *ir_array = array_get_data(ir_state->ir_list);

		*ir = ir_array[index];
	}
	
	return index;
}

int ir_stack_depth(ir_state_t *ir_state){

	return ir_state->stack_index;
}

int ir_push_opcode(ir_state_t *ir_state, uint8_t opcode){

	array_add(&ir_state->opcode_list, &opcode);

	#ifdef IR_ENABLE_DEBUG_LISTING
	ir_debug_listing_t debug = {
		opcode,
		ir_state->current_line,
	};

	array_add(&ir_state->debug_listing, &debug);
	#endif	

	ir_state->last_opcode = opcode;

	return 0;
}

int ir_push_param(ir_state_t *ir_state, uint8_t param){

	array_add(&ir_state->opcode_list, &param);

	return 0;
}

int ir_push_const(ir_state_t *ir_state, int32_t value){

	// check for constant already in array
    const int32_t *constants = (int32_t *)array_get_data(ir_state->const_list);
    uint8_t count = array_get_count(ir_state->const_list);

    int index = -1;

    for(int i = 0; i < count; i++){

        if(value == constants[i]){

            // constant already in array
            index = i;
            break;
        }
    }

    if(index < 0){

        // not found, adding to end of array
        // current count (BEFORE we add the new value)
        // is the new index
        index = count;
        array_add(&ir_state->const_list, &value);
    }


    // #define OPT_PUSHCONST2

    #ifdef OPT_PUSHCONST2
    uint8_t *opcodes = (uint8_t *)array_get_data(ir_state->opcode_list);
    uint16_t opcode_count = array_get_count(ir_state->opcode_list);

    // check previous opcode for push const
    if(ir_state->last_opcode == OPCODE_PUSH_CONST){

    	// printf("$$$$$$$$$$ PC2 %d -> %d\n", opcodes[opcode_count - 2], constants[opcodes[opcode_count - 1]]);

    	// replace with new instruction and push param only
    	opcodes[opcode_count - 2] = OPCODE_PUSH_CONST2;
    	ir_state->last_opcode = OPCODE_PUSH_CONST2;

    	// add index
	    uint8_t op_idx = index;
	    ir_push_param(ir_state, op_idx);
    }
    else{
    #endif

	    ir_push_opcode(ir_state, OPCODE_PUSH_CONST);	    
	   
	    // add index
	    uint8_t op_idx = index;
	    ir_push_param(ir_state, op_idx);
	#ifdef OPT_PUSHCONST2
	}
	#endif

	return 0;
}

int ir_create_jump_target(ir_state_t *ir_state, int16_t label_index){

    if(label_index < 0){

        return -1;
    }

    // label exceeds end of array, we need to create more labels
    if(label_index >= array_get_count(ir_state->jump_targets)){

        jump_target_t jump_target = {0};

        // add labels up to index
        while(array_get_count(ir_state->jump_targets) <= label_index){

            array_add(&ir_state->jump_targets, &jump_target);
        }
    }

    return label_index;
}

void ir_print_listing(ir_state_t *ir_state){

	uint8_t *instructions = (uint8_t *)array_get_data(ir_state->opcode_list);
    int32_t *constants   = (int32_t *)array_get_data(ir_state->const_list);
    jump_target_t *jump_targets = (jump_target_t *)array_get_data(ir_state->jump_targets);

    vm_print_listing(
    	instructions,
    	array_get_count(ir_state->opcode_list),
    	constants,
    	array_get_count(ir_state->const_list),
    	jump_targets,
    	array_get_count(ir_state->jump_targets)
    );
}

void ir_print_debug_listing(ir_state_t *ir_state){

	#ifdef IR_ENABLE_DEBUG_LISTING

	FILE* fp = fopen(ir_state->source_fname, "r");

	if(fp == NULL){

        printf("File read error\n");

        return;
    }

    printf("------------------------------\n");
    printf("Debug listing:\n");

	const ir_debug_listing_t* debug = (ir_debug_listing_t*)array_get_data(ir_state->debug_listing);

	char *line = NULL;
	size_t line_len = 0;
	int line_num = 0;

	while(getline(&line, &line_len, fp) != -1){

		line_num++;

		printf("%4d: %s", line_num, line);

		for(int i = 0; i < array_get_count(ir_state->debug_listing); i++){

			if(debug[i].line_num == line_num){

				printf("\t%s\n", vm_get_opcode_str(debug[i].opcode));
			}

			// printf("%d -> %d\n", debug[i].opcode, debug[i].line_num);
		}
	}

	printf("------------------------------\n");

	fclose(fp);

	#endif
}


// int ir_convert_opcodes(ir_state_t *ir_state){

// 	printf("Convert opcodes\n");

// 	uint8_t *instructions 		= (uint8_t *)array_get_data(ir_state->opcode_list);
// 	uint16_t instruction_count 	= array_get_count(ir_state->opcode_list);
// 	int32_t *constants   		= (int32_t *)array_get_data(ir_state->const_list);

// 	while(instruction_count > 0){

// 		uint8_t opcode = *instructions;

// 		int size = vm_get_instruction_size(opcode);

// 		if(size < 0){

// 			printf("Invalid opcode!\n");
// 			return -1;
// 		}

		
// 		if(opcode == OPCODE_STORE_OBJ){

// 			// opcode sequence:
// 			// push obj
// 			// push field
// 			// push val
// 			// opcode

// 			uint32_t obj_hash = (uint32_t)constants[instructions[-5]];
// 			uint32_t field_hash = (uint32_t)constants[instructions[-3]];

// 			obj_def_t obj_def = {0};

// 			if(ir_lookup_obj_field(ir_state, obj_hash, field_hash, &obj_def) < 0){

// 				return -1;
// 			}

// 			if(obj_def.name_hash == hash_u32_string("pixelarray")){

// 				// this is a pixel array
// 				if(hash_u32_string("hue") == field_hash){

// 					// replace opcode
// 					*instructions = OPCODE_VSTORE_HUE;

// 					// replace push for field
// 					instructions[-3] = OPCODE_TRAP;
// 					instructions[-4] = OPCODE_TRAP;

// 				}
// 			}
// 		}

// 		instructions 		+= size;
// 		instruction_count 	-= size;
// 	}

// 	return 0;
// }


int ir_render_vm(ir_state_t *ir_state, vm_t *vm){

	// assumes VM is inited!

	uint8_t *instructions 		= (uint8_t *)array_get_data(ir_state->opcode_list);
	uint16_t instruction_count 	= array_get_count(ir_state->opcode_list);

	while(instruction_count > 0){

		uint8_t opcode = *instructions;

		int size = vm_get_instruction_size(opcode);

		if(size < 0){

			printf("Invalid opcode: 0x%02x %d\n", opcode, opcode);
			return -1;
		}

		if(opcode != OPCODE_TRAP){

			for(int i = 0; i < size; i++){

				array_add(&vm->instruction_list, &instructions[i]);
			}
		}

		instructions 		+= size;
		instruction_count 	-= size;
	}

	int32_t *constants   		= (int32_t *)array_get_data(ir_state->const_list);
	uint16_t const_count 		= array_get_count(ir_state->const_list);

	for(int i = 0; i < const_count; i++){

		array_add(&vm->constants_list, &constants[i]);
	}

	int32_t zero = 0;

	for(int i = 0; i < ir_state->global_count; i++){

		array_add(&vm->globals_list, &zero);
	}

	for(int i = 0; i < ir_state->locals_count; i++){

		array_add(&vm->locals_list, &zero);
	}

	jump_target_t *jump_targets = (jump_target_t *)array_get_data(ir_state->jump_targets);
	uint16_t jump_count 		= array_get_count(ir_state->jump_targets);

	for(int i = 0; i < jump_count; i++){

		array_add(&vm->jump_targets, &jump_targets[i]);
	}

	return 0;
}


static bool is_obj_type(data_type_t type){

	return (type & TYPE_OBJECT) || (type == TYPE_OBJ_FIELD);
}

int pass_1_bracket(ir_state_t *ir_state, ir_t *ir){

	debug_printf("BRACKET\n");

	/*
	
	Array construction for ident (load):
	
	number b = a[1, 2];

	IR: PUSH_CONST = 1 @ 5
	IR: PUSH_CONST = 2 @ 5
	IR: BRACKET @ 5
	IR: IDENT = a @ 5

	*/

	// check for ident
	if(ir[1].type == IR_IDENT){

		// load path

		var_info_t var_info = {0};

		// lookup var
		if(is_obj_type(ir[1].data_type)){

			debug_printf("\tBracket obj load, rank: %d\n", ir->args.bracket.exprs);
		}
		else{

			if(ir_lookup_symbol(ir_state, ir[1].args.ident.ident_hash, &var_info) < 0){

				printf("IDENT not found\n");

				return -1;
			}

			// check type for array!
			if((var_info.type & TYPE_ARRAY) == 0){

				printf("Not an array type! ident: %s type: %d\n", hash_s_get_str(ir[1].args.ident.ident_hash), ir[1].data_type);

				return -1;
			}
		
			debug_printf("\tBracket load, rank: %d\n", ir->args.bracket.exprs);

			// indexes are already pushed to stack by this point
			// operands:
			// number of indexes
			// global index
			ir_push_const(ir_state, ir->args.bracket.exprs);
			ir_push_const(ir_state, var_info.addr);
		}
	}
	else if(ir[1].type == IR_LVALUE){

		// store path

		// lookup var
		if(is_obj_type(ir[1].data_type)){

			debug_printf("\tBracket obj load, rank: %d\n", ir->args.bracket.exprs);
		}
		else{

			var_info_t var_info = {0};

			// lookup var
			if(ir_lookup_symbol(ir_state, ir[1].args.ident.ident_hash, &var_info) < 0){

				printf("LVALUE not found\n");

				return -1;
			}

			// check type for array!
			if((var_info.type & TYPE_ARRAY) == 0){

				printf("Not an array type!\n");

				return -1;
			}

			debug_printf("Bracket store, rank: %d\n", ir->args.bracket.exprs);

			ir_push_const(ir_state, ir->args.bracket.exprs);
			ir_push_const(ir_state, var_info.addr);
		}
	}
	else{

		printf("Invalid bracket construction\n");

		return -1;
	}

	return 0;
}

int pass_1_dot(ir_state_t *ir_state, ir_t *ir){

	debug_printf("DOT\n");

	debug_printf("\tobj: %s.%s %d:%d\n", 
		hash_s_get_str(ir->args.dot.obj_hash), 
		hash_s_get_str(ir->args.dot.field_hash),
		ir->args.dot.obj_rank,
		ir->args.dot.field_rank);

	obj_def_t obj_def = {0};

	if(ir_lookup_obj_field(ir_state, ir->args.dot.obj_hash, ir->args.dot.field_hash, &obj_def) < 0){

		return -1;
	}

	if(ir->args.dot.store){

	}
	else if((ir->args.dot.obj_rank > 0) || (ir->args.dot.field_rank > 0)){

		if(obj_def.name_hash == HASH32(pixelarray)){

			if(ir->args.dot.field_rank > 0){

				printf("pixelarray index goes on object, not field.\n");

				return -1;
			}
			else if(ir->args.dot.obj_rank > 2){

				printf("Too many indexes for pixelarray\n");
				return -1;
			}

			if(ir->args.dot.obj_rank == 1){

				ir_push_opcode(ir_state, OPCODE_PIXEL_LOAD);
			}
			else if(ir->args.dot.obj_rank == 2){

				ir_push_opcode(ir_state, OPCODE_PIXEL_LOAD2);
			}

			if(ir->args.dot.field_hash == HASH32(hue)){

				ir_push_param(ir_state, PIXEL_ATTR_HUE);
			}
			else if(ir->args.dot.field_hash == HASH32(sat)){

				ir_push_param(ir_state, PIXEL_ATTR_SAT);
			}
			else if(ir->args.dot.field_hash == HASH32(val)){

				ir_push_param(ir_state, PIXEL_ATTR_VAL);
			}
			else if(ir->args.dot.field_hash == HASH32(hsfade)){

				ir_push_opcode(ir_state, PIXEL_ATTR_HSFADE);
			}
			else if(ir->args.dot.field_hash == HASH32(vfade)){

				ir_push_opcode(ir_state, PIXEL_ATTR_VFADE);
			}
			else{

				printf("Unknown pixelarray field: %s\n", hash_s_get_str(ir->args.dot.field_hash));
				return -1;
			}
		}
		else{

			ir_push_opcode(ir_state, OPCODE_LOAD_OBJ_N);	
			ir_push_param(ir_state, ir->args.dot.obj_rank);
			ir_push_param(ir_state, ir->args.dot.field_rank);
		}
	}
	else{

		if(obj_def.name_hash == HASH32(pixelarray)){

			ir_push_opcode(ir_state, OPCODE_PIXEL_VECTOR_LOAD);

			if(ir->args.dot.field_hash == HASH32(count)){

				ir_push_param(ir_state, PIXEL_ATTR_COUNT);
			}
			else if(ir->args.dot.field_hash == HASH32(index)){

				ir_push_param(ir_state, PIXEL_ATTR_INDEX);
			}
			else if(ir->args.dot.field_hash == HASH32(size_x)){

				ir_push_param(ir_state, PIXEL_ATTR_SIZE_X);
			}
			else if(ir->args.dot.field_hash == HASH32(size_y)){

				ir_push_param(ir_state, PIXEL_ATTR_SIZE_Y);
			}
			else if(ir->args.dot.field_hash == HASH32(reverse)){

				ir_push_param(ir_state, PIXEL_ATTR_REVERSE);
			}
			else{

				printf("Unknown pixelarray field: %s\n", hash_s_get_str(ir->args.dot.field_hash));
				return -1;
			}
		}
		else{

			ir_push_opcode(ir_state, OPCODE_LOAD_OBJ);	
		}
	}

	return 0;
}

int pass_1_ident(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_ident_t *arg = &ir->args.ident;

	debug_printf("IDENT: %s\n", hash_s_get_str(arg->ident_hash));

	// printf("%d\n", ir_state->func_index);

	var_info_t var_info = {0};

	// lookup var
	if(ir_lookup_symbol(ir_state, arg->ident_hash, &var_info) < 0){

		// printf("IDENT not found\n");

		// return -1;
	}

	func_def_t func_def = {0};

	if(is_obj_type(ir->data_type)){

		if(ir[2].type == IR_DOT){

			obj_def_t obj_def = {0};

			if(ir_lookup_obj_field(ir_state, arg->ident_hash, 0, &obj_def) < 0){

				return -1;
			}

			// check if pixelarray type
			if(obj_def.name_hash == HASH32(pixelarray)){

				// look ahead
				if(ir[1].type == IR_LVALUE){

					ir[1].type = IR_NOP; // skip loading the field hash to the stack
				}
				else if(ir[1].type == IR_IDENT){

					ir[1].type = IR_NOP; // skip loading the field hash to the stack
				}
			}
		}

		if(var_info.is_ref){

			if(var_info.func_index < 0){
				// global

				// ir_push_const(ir_state, var_info.addr);
				ir_push_opcode(ir_state, OPCODE_LOAD_GLOBAL);		
				ir_push_param(ir_state, var_info.addr);
			}
			else{

				// local
				if(ir_lookup_func_by_index(ir_state, var_info.func_index, &func_def) < 0){

					return -1;
				}

				// ir_push_const(ir_state, var_info.addr);
				ir_push_opcode(ir_state, OPCODE_LOAD_LOCAL);		
				ir_push_param(ir_state, var_info.addr);
			}
		}
		else{

			ir_push_const(ir_state, arg->ident_hash);		
		}
	}
	else if(ir->data_type == TYPE_REF_FUNCTION){

		int32_t index = ir_lookup_func(ir_state, arg->ident_hash, &func_def);

		if(index < 0){

			return -1;
		}

		func_meta_t func_meta = {
			func_def.label_index,
			func_def.locals_index,
		};

		uint32_t val = 0;
		memcpy(&val, &func_meta, sizeof(val));

		ir_push_const(ir_state, val);
	}
	else{

		var_info_t var_info = {0};

		if(ir_lookup_symbol(ir_state, arg->ident_hash, &var_info) < 0){

			printf("IDENT not found\n");

			return -1;
		}

		/*
		
		Array construction for ident (load):
		
		number b = a[1, 2];

		IR: PUSH_CONST = 1 @ 5
		IR: PUSH_CONST = 2 @ 5
		IR: BRACKET @ 5
		IR: IDENT = a @ 5

		*/

		
		// check if scalar
		if((ir->data_type & TYPE_ARRAY) == 0){

			// add opcode
			if(var_info.func_index < 0){
				// global

				ir_push_opcode(ir_state, OPCODE_LOAD_GLOBAL);		
				ir_push_param(ir_state, var_info.addr);
			}
			else{

				// local

				if(ir_lookup_func_by_index(ir_state, var_info.func_index, &func_def) < 0){

					return -1;
				}

				ir_push_opcode(ir_state, OPCODE_LOAD_LOCAL);		
				ir_push_param(ir_state, var_info.addr);
			}
		}
		// array:
		else{

			// check if previous IR is a bracket.
			if(ir[-1].type == IR_BRACKET){

				ir_push_opcode(ir_state, OPCODE_LOAD_ARRAY);	
			}
			// if it isn't, then we need to load the array meta data -
			// basically loading the referenced value.
			else{

				// add opcode
				if(var_info.func_index < 0){
					// global

					ir_push_opcode(ir_state, OPCODE_LOAD_GLOBAL);		
					ir_push_param(ir_state, var_info.addr);
				}
				else{

					// local

					if(ir_lookup_func_by_index(ir_state, var_info.func_index, &func_def) < 0){

						return -1;
					}

					ir_push_opcode(ir_state, OPCODE_LOAD_LOCAL);		
					ir_push_param(ir_state, var_info.addr);
				}
			}

			// if the previous IR *is* a bracket, then we are doing an indexed array load
			// and the bracket code path will handle that.
		}
	}

	return 0;
}

int pass_1_lvalue(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_ident_t *arg = &ir->args.ident;

	debug_printf("LVALUE: %s\n", hash_s_get_str(arg->ident_hash));

	var_info_t var_info = {0};

	if(arg->assume_resolved){
		

	}
	// lookup var
	else if(ir_lookup_symbol(ir_state, arg->ident_hash, &var_info) < 0){

		printf("LVALUE not found\n");

		return -1;
	}

	// check for ref
	if(var_info.is_ref){

		ir_push_const(ir_state, var_info.addr);
	}
	else if(is_obj_type(ir->data_type)){

		ir_push_const(ir_state, arg->ident_hash);
	}
	else{

		// check if scalar
		if((ir->data_type & TYPE_ARRAY) == 0){

			
			ir_push_const(ir_state, var_info.addr);
		}
		else{

			// array

			// check if vector:
			if(ir->args.ident.is_vector){

				ir_push_const(ir_state, var_info.addr);
			}
		}
	}

	return 0;
}

int pass_1_obj_field(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_obj_t *args = &ir->args.obj;

	debug_printf("OBJ_FIELD: %s.%s\n", hash_s_get_str(args->obj_hash), hash_s_get_str(args->field_hash));

	return 0;
}

int pass_1_assign(ir_state_t *ir_state, ir_t *ir){

	debug_printf("ASSIGN\n");

	debug_printf("%d\n", ir->data_type);

	if(ir->data_type & TYPE_ARRAY){

		data_type_t source_type = ir->args.assign.source_type & ~TYPE_ARRAY;
		data_type_t dest_type = ir->data_type & ~TYPE_ARRAY;

		if((source_type == TYPE_GFX16) || (source_type == TYPE_ANY)){

		}
		else if(source_type != dest_type){

			if((dest_type == TYPE_FIXED16) && (source_type == TYPE_NUMBER)){

				ir_push_opcode(ir_state, OPCODE_CONV0_I32_TO_F16);
			}
			else if((dest_type == TYPE_NUMBER) && (source_type == TYPE_FIXED16)){

				ir_push_opcode(ir_state, OPCODE_CONV0_F16_TO_I32);
			}
			else{

				error_printf(ir->line_num, "Cannot assign type %d to type %d", source_type, dest_type);	

				return -1;
			}
		}

		if(ir->args.assign.is_vector){

			if(ir->args.assign.right_side_values == 1){

				ir_push_opcode(ir_state, OPCODE_STORE_VECTOR);			
			}
			else if(ir->args.assign.right_side_values > 1){

				// debug_printf("MOVE VECTOR\n");
				// return -1;

				ir_push_opcode(ir_state, OPCODE_MOVE_VECTOR);
				ir_push_param(ir_state, ir->args.assign.right_side_values);
			}
			else{

				return -1;
			}
		}
		else{

			ir_push_opcode(ir_state, OPCODE_STORE_ARRAY);		
		}
	}
	else if(ir->data_type == TYPE_OBJECT){

		// if(!ir->args.assign.is_obj){

		// 	printf("Assign should be marked as object\n");

		// 	return -1;
		// }

		obj_def_t obj_def = {0};

		if(ir_lookup_obj_field(ir_state, ir->args.assign.obj_hash, ir->args.assign.field_hash, &obj_def) < 0){

			return -1;
		}

		if((ir->args.assign.obj_rank > 0) || (ir->args.assign.field_rank > 0)){

			// check if pixelarray type
			if(obj_def.name_hash == HASH32(pixelarray)){

				if(ir->args.assign.field_rank > 0){

					printf("pixelarray index goes on object, not field.\n");

					return -1;
				}
				else if(ir->args.assign.obj_rank > 2){

					printf("Too many indexes for pixelarray\n");
					return -1;
				}

				if(ir->args.assign.field_hash == HASH32(hue)){

					if(ir->args.assign.obj_rank == 1){

						ir_push_opcode(ir_state, OPCODE_PSTORE_HUE);
					}
					else if(ir->args.assign.obj_rank == 2){

						ir_push_opcode(ir_state, OPCODE_PSTORE_HUE2);
					}	
					else{

						return -1;
					}
				}
				else if(ir->args.assign.field_hash == HASH32(sat)){

					if(ir->args.assign.obj_rank == 1){

						ir_push_opcode(ir_state, OPCODE_PSTORE_SAT);
					}
					else if(ir->args.assign.obj_rank == 2){

						ir_push_opcode(ir_state, OPCODE_PSTORE_SAT2);
					}	
					else{

						return -1;
					}
				}
				else if(ir->args.assign.field_hash == HASH32(val)){

					if(ir->args.assign.obj_rank == 1){

						ir_push_opcode(ir_state, OPCODE_PSTORE_VAL);
					}
					else if(ir->args.assign.obj_rank == 2){

						ir_push_opcode(ir_state, OPCODE_PSTORE_VAL2);
					}	
					else{

						return -1;
					}
				}
				else if(ir->args.assign.field_hash == HASH32(hsfade)){

					if(ir->args.assign.obj_rank == 1){

						ir_push_opcode(ir_state, OPCODE_PSTORE_HSFADE);
					}
					else if(ir->args.assign.obj_rank == 2){

						ir_push_opcode(ir_state, OPCODE_PSTORE_HSFADE2);
					}	
					else{

						return -1;
					}
				}
				else if(ir->args.assign.field_hash == HASH32(vfade)){

					if(ir->args.assign.obj_rank == 1){

						ir_push_opcode(ir_state, OPCODE_PSTORE_VFADE);
					}
					else if(ir->args.assign.obj_rank == 2){

						ir_push_opcode(ir_state, OPCODE_PSTORE_VFADE2);
					}	
					else{

						return -1;
					}
				}
				else{

					printf("Unknown pixelarray field: %s\n", hash_s_get_str(ir->args.dot.field_hash));
					return -1;
				}
			}
			else{
			
				ir_push_opcode(ir_state, OPCODE_STORE_OBJ_N);
				ir_push_param(ir_state, ir->args.assign.obj_rank);
				ir_push_param(ir_state, ir->args.assign.field_rank);
				ir_push_param(ir_state, ir->args.assign.source_type);
			}
		}
		else{

			if(ir->args.assign.field_hash != 0){
				// check if pixelarray type
				if(obj_def.name_hash == HASH32(pixelarray)){
	

					if(ir->args.assign.field_hash == HASH32(hue)){

						ir_push_opcode(ir_state, OPCODE_VSTORE_HUE);
					}
					else if(ir->args.assign.field_hash == HASH32(sat)){

						ir_push_opcode(ir_state, OPCODE_VSTORE_SAT);
					}
					else if(ir->args.assign.field_hash == HASH32(val)){

						ir_push_opcode(ir_state, OPCODE_VSTORE_VAL);
					}
					else if(ir->args.assign.field_hash == HASH32(hsfade)){

						ir_push_opcode(ir_state, OPCODE_VSTORE_HSFADE);
					}
					else if(ir->args.assign.field_hash == HASH32(vfade)){

						ir_push_opcode(ir_state, OPCODE_VSTORE_VFADE);
					}
					else{

						// printf("unknown field: %s.%s\n", hash_s_get_str(ir->args.assign.obj_hash), hash_s_get_str(ir->args.assign.field_hash) );

						// need a swap instruction...

						ir_push_const(ir_state, ir->args.assign.field_hash);

						ir_push_opcode(ir_state, OPCODE_SWAP);

						ir_push_opcode(ir_state, OPCODE_STORE_OBJ);	
						ir_push_param(ir_state, ir->args.assign.source_type);	
					}
				}
				else{

					ir_push_opcode(ir_state, OPCODE_STORE_OBJ);	
					ir_push_param(ir_state, ir->args.assign.source_type);	
				}
			}
			else{

				if(ir->args.assign.is_ref){

					if(ir->args.assign.is_local){

						ir_push_opcode(ir_state, OPCODE_STORE_LOCAL);
					}
					else{

						ir_push_opcode(ir_state, OPCODE_STORE_GLOBAL);
					}
				}
				else{

					ir_push_opcode(ir_state, OPCODE_MOVE_OBJ);
				}
			}
		}	
	}
	// SCALAR
	else{

		// check types
		data_type_t source_type = ir->args.assign.source_type & ~TYPE_ARRAY;

		if((source_type == TYPE_GFX16) || (source_type == TYPE_ANY)){

		}
		else if(source_type != ir->data_type){

			if((ir->data_type == TYPE_FIXED16) && (source_type == TYPE_NUMBER)){

				ir_push_opcode(ir_state, OPCODE_CONV0_I32_TO_F16);
			}
			else if((ir->data_type == TYPE_NUMBER) && (source_type == TYPE_FIXED16)){

				ir_push_opcode(ir_state, OPCODE_CONV0_F16_TO_I32);
			}
			else{

				error_printf(ir->line_num, "Cannot assign type %d to type %d", source_type, ir->data_type);	

				return -1;
			}
		}

		if(ir->args.assign.is_local){

			ir_push_opcode(ir_state, OPCODE_STORE_LOCAL);
		}
		else{

			ir_push_opcode(ir_state, OPCODE_STORE_GLOBAL);
		}
	}

	return 0;
}

int pass_1_binop(ir_state_t *ir_state, ir_t *ir){

	debug_printf("BINOP\n");

	const ir_arg_binop_t *arg = &ir->args.binop;


	// check for conversions:
	if(arg->left_type != arg->right_type){

		if((arg->left_type == TYPE_FIXED16) && (arg->right_type == TYPE_NUMBER)){

			ir_push_opcode(ir_state, OPCODE_CONV0_I32_TO_F16);
		}
		else if((arg->left_type == TYPE_NUMBER) && (arg->right_type == TYPE_FIXED16)){

			ir_push_opcode(ir_state, OPCODE_CONV1_I32_TO_F16);
		}
	}

	uint8_t opcode = 0;

	if(arg->op == IR_OP_ADD){

		opcode = OPCODE_ADD;
	}
	else if(arg->op == IR_OP_SUB){

		opcode = OPCODE_SUB;
	}
	else if(arg->op == IR_OP_MUL){

		if(ir->data_type == TYPE_FIXED16){

			opcode = OPCODE_MUL_F16;
		}
		else{

			opcode = OPCODE_MUL;	
		}
	}
	else if(arg->op == IR_OP_DIV){

		if(ir->data_type == TYPE_FIXED16){

			opcode = OPCODE_DIV_F16;
		}
		else{

			opcode = OPCODE_DIV;	
		}
	}
	else if(arg->op == IR_OP_MOD){

		opcode = OPCODE_MOD;
	}
	else if(arg->op == IR_OP_IN){

		opcode = OPCODE_COMPLT;
	}
	else if(arg->op == IR_OP_COMPEQ){

		opcode = OPCODE_COMPEQ;
	}
	else if(arg->op == IR_OP_COMPNEQ){

		opcode = OPCODE_COMPNEQ;
	}
	else if(arg->op == IR_OP_COMPLT){

		opcode = OPCODE_COMPLT;
	}
	else if(arg->op == IR_OP_COMPLTE){

		opcode = OPCODE_COMPLTE;
	}
	else if(arg->op == IR_OP_COMPGT){

		opcode = OPCODE_COMPGT;
	}
	else if(arg->op == IR_OP_COMPGTE){

		opcode = OPCODE_COMPGTE;
	}
	else if(arg->op == IR_OP_LOGICAL_AND){

		opcode = OPCODE_LOGICAL_AND;
	}
	else if(arg->op == IR_OP_LOGICAL_OR){

		opcode = OPCODE_LOGICAL_OR;
	}
	else{

		printf("ERROR unknown binop: %d\n", arg->op);
		return -1;
	}

	// add opcode
	ir_push_opcode(ir_state, opcode);

	return 0;
}

int pass_1_assign_op(ir_state_t *ir_state, ir_t *ir){

	debug_printf("ASSIGN_OP\n");

	const ir_arg_assign_op_t *arg = &ir->args.assign_op;

	// check for conversions:
	data_type_t left_type = arg->left_type & ~TYPE_ARRAY;
	data_type_t right_type = arg->right_type & ~TYPE_ARRAY;

	if(left_type != right_type){

		if((left_type == TYPE_FIXED16) && (right_type == TYPE_NUMBER)){

			ir_push_opcode(ir_state, OPCODE_CONV0_I32_TO_F16);
		}
		else if((left_type == TYPE_NUMBER) && (right_type == TYPE_FIXED16)){

			ir_push_opcode(ir_state, OPCODE_CONV0_F16_TO_I32);
		}
	}

	uint8_t opcode = 0;
	uint8_t op = ASSIGN_OP_NONE;

	if(arg->op == IR_OP_ADD){

		op = ASSIGN_OP_ADD;
	}
	else if(arg->op == IR_OP_SUB){

		op = ASSIGN_OP_SUB;
	}
	else if(arg->op == IR_OP_MUL){

		op = ASSIGN_OP_MUL;
	}
	else if(arg->op == IR_OP_DIV){

		op = ASSIGN_OP_DIV;
	}
	else if(arg->op == IR_OP_MOD){

		op = ASSIGN_OP_MOD;
	}
	else{

		error_printf(ir->line_num, "Unknown assign op");

		return -1;
	}

	obj_def_t obj_def = {0};

	// objects:
	if(arg->is_obj){

		debug_printf("assign op %s\n", hash_s_get_str(arg->obj_hash));

		if(ir_lookup_obj_field(ir_state, arg->obj_hash, arg->field_hash, &obj_def) < 0){

			return -1;
		}

		if(obj_def.name_hash == HASH32(pixelarray)){

			if(arg->obj_rank > 2){

				printf("Too many indexes for pixelarray!\n");
				return -1;
			}

			if(arg->obj_rank == 1){

				if(arg->field_hash == HASH32(hue)){

					opcode = OPCODE_PIXEL_OP_HUE;
				}
				else if(arg->field_hash == HASH32(sat)){

					opcode = OPCODE_PIXEL_OP_SAT;
				}
				else if(arg->field_hash == HASH32(val)){

					opcode = OPCODE_PIXEL_OP_VAL;
				}
				else{

					printf("Unknown field for pixelarray: %s\n", hash_s_get_str(arg->field_hash));
				}
			}
			else if(arg->obj_rank == 2){

				if(arg->field_hash == HASH32(hue)){

					opcode = OPCODE_PIXEL_OP_HUE2;
				}
				else if(arg->field_hash == HASH32(sat)){

					opcode = OPCODE_PIXEL_OP_SAT2;
				}
				else if(arg->field_hash == HASH32(val)){

					opcode = OPCODE_PIXEL_OP_VAL2;
				}
				else{

					printf("Unknown field for pixelarray: %s\n", hash_s_get_str(arg->field_hash));
				}
			}
			else{

				if(arg->field_hash == HASH32(hue)){

					opcode = OPCODE_PIXEL_VECTOR_OP_HUE;
				}
				else if(arg->field_hash == HASH32(sat)){

					opcode = OPCODE_PIXEL_VECTOR_OP_SAT;
				}
				else if(arg->field_hash == HASH32(val)){

					opcode = OPCODE_PIXEL_VECTOR_OP_VAL;
				}
				else{

					printf("Unknown field for pixelarray: %s\n", hash_s_get_str(arg->field_hash));
				}
			}
		}
		else{

			if(arg->obj_rank > 0){

				opcode = OPCODE_ASSIGN_OP_OBJ_N;	
			}
			else{

				opcode = OPCODE_ASSIGN_OP_OBJ;	
			}
		}
	}
	// arrays:
	else if(ir->data_type & TYPE_ARRAY){

		if(ir->args.assign_op.is_vector){

			opcode = OPCODE_ASSIGN_OP_VECTOR;
		}
		else{

			opcode = OPCODE_ASSIGN_OP_ARRAY;	
		}
	}
	// scalars:
	else if(arg->op == IR_OP_ADD){

		if(arg->is_local){

			opcode = OPCODE_ADD_ASSIGN_LOCAL;
		}
		else{

			opcode = OPCODE_ADD_ASSIGN;	
		}
	}
	else if(arg->op == IR_OP_SUB){

		if(arg->is_local){

			opcode = OPCODE_SUB_ASSIGN_LOCAL;
		}
		else{

			opcode = OPCODE_SUB_ASSIGN;	
		}
	}
	else if(arg->op == IR_OP_MUL){

		if(arg->is_local){

			opcode = OPCODE_MUL_ASSIGN_LOCAL;
		}
		else{

			opcode = OPCODE_MUL_ASSIGN;	
		}
	}
	else if(arg->op == IR_OP_DIV){

		if(arg->is_local){

			opcode = OPCODE_DIV_ASSIGN_LOCAL;
		}
		else{

			opcode = OPCODE_DIV_ASSIGN;	
		}
	}
	else if(arg->op == IR_OP_MOD){

		if(arg->is_local){

			opcode = OPCODE_MOD_ASSIGN_LOCAL;
		}
		else{

			opcode = OPCODE_MOD_ASSIGN;	
		}
	}
	else{

		printf("ERROR unknown assign op: %d\n", arg->op);
		return -1;
	}

	if(opcode == 0){

		return -1;
	}

	// add opcode
	ir_push_opcode(ir_state, opcode);

	if(arg->is_obj || (ir->data_type & TYPE_ARRAY)){

		// push op for combined ops
		ir_push_param(ir_state, op);
	}

	if(obj_def.name_hash == HASH32(pixelarray)){

	}
	else{

		if((arg->obj_rank > 0) || (arg->field_rank > 0)){

			ir_push_param(ir_state, arg->obj_rank);
			ir_push_param(ir_state, arg->field_rank);
		}
	}

	return 0;
}	

int pass_1_end_expr(ir_state_t *ir_state, ir_t *ir){

	debug_printf("END_EXPR\n");

	return 0;
}	

int pass_1_push_const(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_const_t *arg = &ir->args.constant;

	debug_printf("CONST: %d\n", arg->value);

	ir_push_const(ir_state, arg->value);

	return 0;
}

int pass_1_call(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_call_t *args = &ir->args.call;

	debug_printf("CALL: %s\n", hash_s_get_str(args->ident_hash));

	// look up function and get return type
	func_def_t func_def = {0};
	bool libcall = true;

	// check if indirect
	if(args->indirect){

		libcall = false;
	}
	else{

		int func_index = ir_lookup_libfunc(ir_state, args->ident_hash, &func_def);

		if(func_index < 0){

			libcall = false;

			func_index = ir_lookup_func(ir_state, args->ident_hash, &func_def);
		}

		if(func_index < 0){

			printf("Function not found!\n");
			return -1;
		}
	}

	if(args->indirect){

		// lookup symbol
		int symbol_idx = ir_lookup_symbol(ir_state, args->ident_hash, NULL);

		if(symbol_idx < 0){

			printf("Indirect call symbol not found!\n");

			return -1;
		}

		ir_push_opcode(ir_state, OPCODE_ICALL);	
		ir_push_param(ir_state, args->arg_count);
		ir_push_param(ir_state, symbol_idx);  // !!!!!!! this is probably a bug!
	}
	else if(libcall){

		// check arg count
		if((args->arg_count < func_def.arg_count_min) || (args->arg_count > func_def.arg_count_max)){
			printf("Incorrect number of arguments\n");

			return -1;
		}

		ir_push_const(ir_state, func_def.name_hash);
		
		ir_push_opcode(ir_state, OPCODE_CALL_NATIVE);	
		ir_push_param(ir_state, args->arg_count);
	}
	else{

		if(args->arg_count != func_def.arg_count){

			printf("Incorrect number of arguments\n");

			return -1;
		}

		if(func_def.is_coroutine){
			
			ir_push_opcode(ir_state, OPCODE_CORO_INIT);
			ir_push_param(ir_state, args->arg_count);
			ir_push_param(ir_state, func_def.locals_size);
			ir_push_param(ir_state, func_def.label_index);
		}
		else{

			ir_push_opcode(ir_state, OPCODE_CALL);	
			ir_push_param(ir_state, args->arg_count);
			ir_push_param(ir_state, func_def.locals_index);
			ir_push_param(ir_state, func_def.label_index);		
		}
	}

	// check if function is discarding a param
	if(args->discard){

		ir_push_opcode(ir_state, OPCODE_DISCARD);
	}
	
	// check if function is returning a value
	if(func_def.return_type != TYPE_NONE){

		// returning a value
		// check if value is used before loading return value to stack
		if(!args->unused_return){

			ir_push_opcode(ir_state, OPCODE_LOAD_RETURN);
		}
	}

	return 0;
}

int pass_1_return(ir_state_t *ir_state, ir_t *ir){

	debug_printf("RETURN\n");
	debug_printf("expressions: %d\n", ir->args.ret.return_expressions);

	// look up function and get return type
	const func_def_t *func = array_get_data(ir_state->functions);

	if(func[ir_state->func_index].return_type != TYPE_NONE){

		// check that we are returning the correct number of expressions
		if(ir->args.ret.return_expressions != 1){

			printf("Invalid number of return expressions\n");

			return -1;
		}
	}
	// returning none:
	else{

		if(ir->args.ret.return_expressions != 0){

			printf("Invalid number of return expressions\n");

			return -1;
		}
	}

	// check if coroutine
	if(func[ir_state->func_index].is_coroutine){

		if(ir->args.ret.return_expressions != 0){

			printf("Coroutines cannot return values\n");

			return -1;
		}

		ir_push_opcode(ir_state, OPCODE_CORO_EXIT);
	}
	else{

		if(ir->args.ret.return_expressions == 0){

			ir_push_opcode(ir_state, OPCODE_RETURN_VOID);		
		}
		else{

			ir_push_opcode(ir_state, OPCODE_RETURN);	
		}
	}

	return 0;
}


int pass_1_yield(ir_state_t *ir_state, ir_t *ir){

	debug_printf("YIELD\n");

	ir_push_opcode(ir_state, OPCODE_YIELD);	

	return 0;
}

int pass_1_delay(ir_state_t *ir_state, ir_t *ir){

	debug_printf("DELAY\n");

	ir_push_opcode(ir_state, OPCODE_DELAY);	

	return 0;
}

int pass_1_rate(ir_state_t *ir_state, ir_t *ir){

	debug_printf("RATE\n");

	ir_push_opcode(ir_state, OPCODE_RATE);	

	return 0;
}

int pass_1_publish(ir_state_t *ir_state, ir_t *ir){

	debug_printf("PUBLISH\n");

	var_info_t var_info = {0};

	if(ir_lookup_symbol(ir_state, ir->args.ident.ident_hash, &var_info) < 0){

		printf("ERROR\n");
		return -1;
	}

	ir_push_const(ir_state, ir->args.ident.ident_hash);

	ir_push_opcode(ir_state, OPCODE_PUBLISH);
	ir_push_param(ir_state, var_info.addr);	
	ir_push_param(ir_state, var_info.type);	

	return 0;
}

int pass_1_jump(ir_state_t *ir_state, ir_t *ir){

	// lookup label number, create if not exists
	int label = ir_create_jump_target(ir_state, ir->args.jump.label_index);

	if(label < 0){

		printf("Failed to create label\n");

		return -1;
	}

	debug_printf("JUMP -> %d\n", label);

	if(ir->args.jump.jump == IR_JUMP_ZERO){

		ir_push_opcode(ir_state, OPCODE_JUMP_Z);
	}
	else if(ir->args.jump.jump == IR_JUMP_NOT_ZERO){

		ir_push_opcode(ir_state, OPCODE_JUMP_NZ);
	}
	else if(ir->args.jump.jump == IR_JUMP_ALWAYS){

		ir_push_opcode(ir_state, OPCODE_JUMP);
	}
	else{

		printf("Unknown jump type\n");

		return -1;
	}

	ir_push_param(ir_state, label);

	return 0;
}

int pass_1_label(ir_state_t *ir_state, ir_t *ir){

	// lookup label number, create if not exists
	int label = ir_create_jump_target(ir_state, ir->args.label.label_index);

	if(label < 0){

		return -1;
	}

	debug_printf("LABEL <- %d\n", label);

	// get instruction offset
	uint16_t ins_offset = array_get_count(ir_state->opcode_list);

	// update label with offset
	if(label >= array_get_count(ir_state->jump_targets)){

		printf("Label out of bounds!\n");
		return -1;
	}

	jump_target_t *jump_targets = (jump_target_t *)array_get_data(ir_state->jump_targets);

	jump_targets[label].instruction_offset = ins_offset;

	return 0;
}

int pass_1_func(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_func_t *args = &ir->args.func;

	if(args->is_coroutine){

		debug_printf("CORO: %s\n", hash_s_get_str(args->ident_hash));
	}
	else{

		debug_printf("FUNC: %s\n", hash_s_get_str(args->ident_hash));	
	}

	func_def_t *func = array_get_data(ir_state->functions);

	ir_state->func_index = 0;
	while(ir_state->func_index < array_get_count(ir_state->functions)){

		if(func->name_hash == args->ident_hash){

			break;
		}

		ir_state->func_index++;
		func++;
	}

	if(ir_state->func_index >= array_get_count(ir_state->functions)){

		printf("ERROR (pass_1_func)\n");

		return -1;
	}

	ir_push_opcode(ir_state, OPCODE_FUNC);

	// set function offset
	func->instruction_offset = array_get_count(ir_state->opcode_list);

	ir_create_jump_target(ir_state, args->label_index);

	if(args->label_index >= array_get_count(ir_state->jump_targets)){

		printf("Label out of bounds!\n");
		return -1;
	}

	jump_target_t *jump_targets = array_get_data(ir_state->jump_targets);
	jump_targets[args->label_index].instruction_offset = func->instruction_offset;

	return 0;
}

int pass_1_end_func(ir_state_t *ir_state, ir_t *ir){

	// set instruction count for this function
	func_def_t *func = array_get_data(ir_state->functions);
	func[ir_state->func_index].instruction_count = array_get_count(ir_state->opcode_list) - func[ir_state->func_index].instruction_offset;

	debug_printf("END_FUNC: %d @ %d len %d\n", ir_state->func_index, func[ir_state->func_index].instruction_offset, func[ir_state->func_index].instruction_count);

	// reset func index to global
	ir_state->func_index = -1;

	ir_push_opcode(ir_state, OPCODE_END_FUNC);	

	return 0;
}

int pass_1_declare(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_declare_t *args = &ir->args.declare;

	var_info_t var_info = {0};

	if(ir_lookup_symbol(ir_state, args->ident_hash, &var_info) < 0){

		printf("Not declared: %s\n", hash_s_get_str(args->ident_hash));
		return -1;
	}

	// check if ref
	if(var_info.is_ref){


	}
	// check if object
	else if(var_info.type & TYPE_OBJECT){

		if(args->rank > 0){

			printf("Rank cannot be above 0 for an object\n");

			return -1;
		}

		// uint8_t obj_type = var_info.type & MASK_OBJECT;
		// debug_printf("obj type: %d\n", obj_type);
		debug_printf("obj info index: %d\n", var_info.obj_index);

		// get object meta data
		obj_def_t *obj_ptr = array_get_data(ir_state->objects);
		const obj_def_t *obj = &obj_ptr[var_info.obj_index];

		ir_push_const(ir_state, args->ident_hash);
		ir_push_const(ir_state, obj->name_hash);
		
		ir_push_opcode(ir_state, OPCODE_OBJ_SIZE);
	}
	// check if array
	else if(args->rank > 0){

		// push rank
		ir_push_const(ir_state, args->rank);

		// push index of symbol
		ir_push_const(ir_state, var_info.addr);
		
		ir_push_opcode(ir_state, OPCODE_ARRAY_SIZE);
	}

	return 0;
}


int pass_1_loop_init(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_loop_init_t *args = &ir->args.loop_init;

	debug_printf("LOOP_INIT: %s\n", hash_s_get_str(args->ident_hash));

	var_info_t var_info = {0};

	if(ir_lookup_symbol(ir_state, args->ident_hash, &var_info) < 0){

		printf("IDENT not found\n");

		return -1;
	}
	
	// push index of symbol
	
	// global - not allowed
	if(var_info.func_index < 0){

		return -1;
	}
	// local
	else{

		ir_push_const(ir_state, var_info.addr);

		ir_push_opcode(ir_state, OPCODE_ZERO_LOCAL);
	}

	return 0;
}

int pass_1_loop_exit(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_loop_exit_t *args = &ir->args.loop_exit;

	debug_printf("LOOP_EXIT\n");

	var_info_t var_info = {0};

	if(ir_lookup_symbol(ir_state, args->ident_hash, &var_info) < 0){

		printf("IDENT not found\n");

		return -1;
	}

	// push index of symbol
	
	// global - not allowed
	if(ir_state->func_index < 0){

		return -1;
	}
	// local
	else{

		ir_push_const(ir_state, var_info.addr);
	}

	ir_push_opcode(ir_state, OPCODE_LOOP);

	// lookup label number, create if not exists
	int label = ir_create_jump_target(ir_state, args->label_index);

	if(label < 0){

		return -1;
	}

	ir_push_param(ir_state, label);

	return 0;
}

int pass_1_assert(ir_state_t *ir_state, ir_t *ir){

	debug_printf("ASSERT\n");

	ir_push_opcode(ir_state, OPCODE_ASSERT);

	return 0;
}


int ir_pass_1(ir_state_t *ir_state){

	debug_printf("****************\n");
	debug_printf("IR PASS 1\n");

	ir_t *ir = array_get_data(ir_state->ir_list);

	for(int i = 0; i < array_get_count(ir_state->ir_list); i++){

		ir_print(ir);

		ir_state->current_line = ir->line_num;

		switch(ir->type){
			case IR_DECLARE:
				if(pass_1_declare(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_IDENT:
				if(pass_1_ident(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_LVALUE:
				if(pass_1_lvalue(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_ASSERT:
				if(pass_1_assert(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_BRACKET:
				if(pass_1_bracket(ir_state, ir) < 0){

					return -1;
				}
				break;

			case IR_DOT:
				if(pass_1_dot(ir_state, ir) < 0){

					return -1;
				}
				break;

			case IR_OBJ_FIELD:
				if(pass_1_obj_field(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_ASSIGN:
				if(pass_1_assign(ir_state, ir) < 0){

					return -1;
				}
			
				break;

			case IR_BINOP:
				if(pass_1_binop(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_ASSIGN_OP:
				if(pass_1_assign_op(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_END_EXPR:
				if(pass_1_end_expr(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_PUSH_CONST:
				if(pass_1_push_const(ir_state, ir) < 0){

					return -1;
				}
				
				break;

			case IR_CALL:
				if(pass_1_call(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_RETURN:
				if(pass_1_return(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_YIELD:
				if(pass_1_yield(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_DELAY:
				if(pass_1_delay(ir_state, ir) < 0){

					return -1;
				}

				break;
			
			case IR_RATE:
				if(pass_1_rate(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_PUBLISH:
				if(pass_1_publish(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_FUNC:
				if(pass_1_func(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_END_FUNC:
				if(pass_1_end_func(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_JUMP:
				if(pass_1_jump(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_LABEL:
				if(pass_1_label(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_LOOP_INIT:
				if(pass_1_loop_init(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_LOOP_EXIT:
				if(pass_1_loop_exit(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_START:
				break;

			case IR_END:
				break;

			case IR_NOP:
				break;

			default:
				debug_printf("Unknown IR:\n");
				ir_print(ir);
				break;
		}

		ir++;
	}

	debug_printf("END PASS 1\n");
	debug_printf("****************\n");

	// // add halt instruction
	uint8_t opcode = OPCODE_HALT;
	ir_push_opcode(ir_state, opcode);

	debug_printf("\nIR Opcode Listing:\n");
	ir_print_listing(ir_state);
	debug_printf("IR state size: %d\n", ir_get_state_size(ir_state));

    ir_print_symbols(ir_state);
    ir_print_objects(ir_state);
    ir_print_functions(ir_state);
		
	return 0;
}


int pass_0_declare(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_declare_t *args = &ir->args.declare;

	debug_printf("DECLARE: %s type: %d rank: %d\n", hash_s_get_str(args->ident_hash), ir->data_type, args->rank);

	if(ir_lookup_symbol(ir_state, args->ident_hash, NULL) >= 0){

		debug_printf("FUNC LOCAL: %d\n", ir_state->func_index);

		printf("Already declared: %s\n", hash_s_get_str(args->ident_hash));
		return -1;
	}

	// check name against functions objects
	if(ir_lookup_func(ir_state, args->ident_hash, NULL) >= 0){

		printf("Name in use by function: %s\n", hash_s_get_str(args->ident_hash));
		return -1;
	}

	// check if declare is global
	if(ir_state->func_index < 0){

		if(ir_add_global_var(ir_state, args->ident_hash, ir->data_type)){

			return -1;
		}	
	}
	else{

		// function local
		if(ir_add_local_var(ir_state, args->ident_hash, ir->data_type, ir_state->func_index)){

			return -1;
		}
	}
	
	var_info_t *var_ptr = 0;

	if(ir_lookup_symbol_ptr(ir_state, args->ident_hash, &var_ptr) < 0){

		return -1;
	}


	var_ptr->array_rank = args->rank;
	var_ptr->is_ref = args->is_ref;

	// check if object
	if(var_ptr->type & TYPE_OBJECT){

		// set obj index
		var_ptr->obj_index = var_ptr->type & ~TYPE_OBJECT;
	}

	// check if declare is global
	if(ir_state->func_index < 0){

		// set address
		var_ptr->addr = ir_state->global_count;

		// add to vm globals	
		ir_state->global_count++;
	}
	else{
		// function local

		// check if array
		if(var_ptr->type & TYPE_ARRAY){

			printf("Arrays not supported in function locals (yet - see bytecode engine)\n");
				
			return -1;
		}

		func_def_t func_def = {0};

		if(ir_lookup_func_by_index(ir_state, ir_state->func_index, &func_def) < 0){

			return -1;
		}

		if(func_def.is_coroutine){

			var_ptr->addr = func_def.locals_size;

			ir_add_func_local(ir_state, ir_state->func_index);
		}
		else{

			// set address
			var_ptr->addr = ir_state->locals_count - func_def.locals_index;
			
			// add to vm locals
			ir_state->locals_count++;

			ir_add_func_local(ir_state, ir_state->func_index);
		}
	}


	int rank = args->rank;
	while(rank > 0){

		// pop types from stack
		if(ir_pop_stack(ir_state, NULL) < 0){

			return -1;
		}

		rank--;
	}
	
	return 0;
}


int pass_0_push_const(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_const_t *arg = &ir->args.constant;

	debug_printf("CONST: %d type: %d\n", arg->value, ir->data_type);

	// push type to stack
	if(ir_push_stack(ir_state, ir) < 0){

		return -1;
	}

	return 0;
}

int pass_0_ident(ir_state_t *ir_state, ir_t *ir){

	ir_arg_ident_t *arg = &ir->args.ident;

	debug_printf("IDENT: %s\n", hash_s_get_str(arg->ident_hash));

	data_type_t data_type = TYPE_NONE;

	// check if next IR is a dot
	if(ir[1].type == IR_DOT){

		data_type = TYPE_OBJ_FIELD;

		// printf("DOT!!!\n");
	}
	else if(ir_lookup_func(ir_state, arg->ident_hash, NULL) >= 0){

		data_type = TYPE_REF_FUNCTION;

		debug_printf("\tfuncref: %s\n", hash_s_get_str(arg->ident_hash));
	}	
	else if(ir_lookup_type(ir_state, arg->ident_hash, &data_type) < 0){

		printf("Not declared: %s\n", hash_s_get_str(arg->ident_hash));

		return -1;
	}

	// annotate with type
	ir->data_type = data_type;

	if(ir->data_type & TYPE_ARRAY){
		
		// check if previous is bracket
		if(ir[-1].type == IR_BRACKET){

			arg->is_vector = false;
		}
		else{

			arg->is_vector = true;	
		}
	}

	// push to stack
	if(ir_push_stack(ir_state, ir) < 0){

		return -1;
	}

	return 0;
}

int pass_0_lvalue(ir_state_t *ir_state, ir_t *ir){

	ir_arg_ident_t *arg = &ir->args.ident;

	debug_printf("LVALUE: %s\n", hash_s_get_str(arg->ident_hash));

	data_type_t data_type = TYPE_NONE;

	// check if next IR is a dot
	if(ir[1].type == IR_DOT){

		data_type = TYPE_OBJ_FIELD;

		// debug_printf("DOT!!!\n");
	}
	else if(ir_lookup_type(ir_state, arg->ident_hash, &data_type) < 0){

		printf("Not declared: %s\n", hash_s_get_str(arg->ident_hash));

		return -1;
	}

	// annotate with type
	ir->data_type = data_type;

	if(ir->data_type & TYPE_ARRAY){
		
		// check if previous is bracket
		if(ir[-1].type == IR_BRACKET){

			arg->is_vector = false;
		}
		else{

			arg->is_vector = true;	
		}
	}	
	
	// push type to stack
	if(ir_push_stack(ir_state, ir) < 0){

		return -1;
	}

	/*
	
	Array construction for lvalue (store):

	a[1, 2] = 3; 

	where a is number a[4,4]

	IR: PUSH_CONST = 1 @ 4
	IR: PUSH_CONST = 2 @ 4
	IR: BRACKET @ 4
	IR: LVALUE = a @ 4

	*/

	return 0;
}

int pass_0_end_expr(ir_state_t *ir_state, ir_t *ir){

	debug_printf("END_EXPR\n");

	return 0;
}

int pass_0_assign(ir_state_t *ir_state, ir_t *ir){

	debug_printf("ASSIGN\n\t");

	ir_print(ir);

	ir_t left = {0};
	ir_t right = {0};

	int right_val_count = 1;

	if(ir_pop_stack(ir_state, &right) < 0){

		return -1;
	}

	if(ir_pop_stack(ir_state, &left) < 0){

		return -1;
	}

	if((left.type != IR_LVALUE) && (left.type != IR_DOT)){

		while(left.type != IR_LVALUE){

			right = left;
			
			if(ir_pop_stack(ir_state, &left) < 0){

				return -1;
			}

			ir_print(&left);

			right_val_count++;
		}
	}


	debug_printf("\tleft:  ");
	ir_print(&left);
	debug_printf("\tright: ");
	ir_print(&right);
	debug_printf("\tright side items: %d\n", right_val_count);

	// look up target type
	data_type_t target_type = 0;

	var_info_t var_info = {0};

	if(left.type == IR_DOT){

		if(ir_lookup_symbol(ir_state, left.args.dot.obj_hash, &var_info) >= 0){

			// check type
			if(!is_obj_type(var_info.type)){

				printf("Invalid type for dot\n");

				return -1;
			}
		}
		else{

			printf("Assign object not found\n");

			return -1;	
		}

		ir->args.assign.is_obj = true;

		target_type = left.data_type;

		if((left.args.dot.obj_rank > 0) || (left.args.dot.field_rank > 0)){

			ir->args.assign.obj_rank = left.args.dot.obj_rank;
			ir->args.assign.field_rank = left.args.dot.field_rank;
		}

		ir->args.assign.obj_hash = left.args.dot.obj_hash;
		ir->args.assign.field_hash = left.args.dot.field_hash;

	}
	else if(ir_lookup_symbol(ir_state, left.args.ident.ident_hash, &var_info) >= 0){

		target_type = var_info.type;

		ir->args.assign.is_local = var_info.func_index >= 0;
		ir->args.assign.is_vector = left.args.ident.is_vector;
		ir->args.assign.is_ref = var_info.is_ref;
	}
	else{
		printf("Assign target not found\n");

		return -1;
	}

	// assign type to assign hahah
	ir->data_type = target_type;

	ir->args.assign.source_type = right.data_type;
	ir->args.assign.right_side_values = right_val_count;

	return 0;
}

int pass_0_binop(ir_state_t *ir_state, ir_t *ir){

	debug_printf("BINOP\n\t");

	ir_print(ir);

	ir_t left;
	ir_t right;

	if(ir_pop_stack(ir_state, &right) < 0){

		return -1;
	}

	if(ir_pop_stack(ir_state, &left) < 0){

		return -1;
	}

	data_type_t left_type = left.data_type;
	data_type_t right_type = right.data_type;

	debug_printf("\t left: %d | right: %d\n", left_type, right_type);

	debug_printf("\tleft:  ");
	ir_print(&left);
	debug_printf("\tright: ");
	ir_print(&right);


	if(left_type == right_type){

		ir->data_type = left_type;
	}
	// check for array loads
	// this is any time we have a bracketed node on the right side,
	// OR on the left in an op that is NOT an assign.
	// do the right side first:
	else if(right_type & TYPE_ARRAY){

		right_type &= ~TYPE_ARRAY;

		if(right_type != left_type){

			printf("ERROR cannot do type conversion for: %d != %d\n", left_type, right_type);

			return -1;
		}

		ir->data_type = left_type;
	}
	else if(left_type & TYPE_ARRAY){

		left_type &= ~TYPE_ARRAY;

		if(right_type != left_type){

			printf("ERROR cannot do type conversion for: %d != %d\n", left_type, right_type);

			return -1;
		}

		ir->data_type = left_type;
	}
	else if((left_type == TYPE_FIXED16) && (right_type == TYPE_NUMBER)){

		// f16

		ir->data_type = left_type;
	}
	else if((left_type == TYPE_NUMBER) && (right_type == TYPE_FIXED16)){

		// f16

		ir->data_type = right_type;
	}
	else if((left_type == TYPE_GFX16) || (right_type == TYPE_GFX16)){

		// no conversion on gfx16

	}
	else if(left_type == TYPE_ANY){
		
	}
	else{

		printf("ERROR cannot do type conversions yet! %d != %d\n", left_type, right_type);

		return -1;
	}

	if(ir_push_stack(ir_state, ir) < 0){

		return -1;
	}

	ir->args.binop.left_type = left_type;
	ir->args.binop.right_type = right_type;

	return 0;
}

int pass_0_assign_op(ir_state_t *ir_state, ir_t *ir){

	debug_printf("ASSIGN_OP\n\t");

	ir_print(ir);

	ir_t left;
	ir_t right;

	if(ir_pop_stack(ir_state, &right) < 0){

		return -1;
	}

	if(ir_pop_stack(ir_state, &left) < 0){

		return -1;
	}

	data_type_t left_type = left.data_type;
	data_type_t right_type = right.data_type;

	debug_printf("\t left: %d | right: %d\n", left_type, right_type);

	debug_printf("\tleft:  ");
	ir_print(&left);
	debug_printf("\tright: ");
	ir_print(&right);

	if((left.type != IR_IDENT) && (left.type != IR_LVALUE) && (left.type != IR_DOT)){

		return -1;
	}

	// lookup assign target
	var_info_t var_info = {0};

	if(ir_lookup_symbol(ir_state, left.args.ident.ident_hash, &var_info) < 0){

		return -1;
	}

	// check if local or global
	if(var_info.func_index < 0){

		// global
		ir->args.assign_op.is_local = false;
	}
	else{

		// local
		ir->args.assign_op.is_local = true;
	}

	if(left.type == IR_DOT){

		if(ir_lookup_symbol(ir_state, left.args.dot.obj_hash, &var_info) >= 0){

			// check type
			if(!is_obj_type(var_info.type)){

				printf("Invalid type for dot\n");

				return -1;
			}
		}
		else{

			printf("Assign object not found\n");

			return -1;	
		}

		ir->args.assign_op.is_obj = true;

		if((left.args.dot.obj_rank > 0) || (left.args.dot.field_rank > 0)){

			ir->data_type = TYPE_OBJECT;

			ir->args.assign_op.obj_rank = left.args.dot.obj_rank;
			ir->args.assign_op.field_rank = left.args.dot.field_rank;
		}
		else{

			ir->data_type = TYPE_OBJECT;	
		}

		ir->args.assign_op.obj_hash = left.args.dot.obj_hash;
		ir->args.assign_op.field_hash = left.args.dot.field_hash;
	}
	else if(left_type == right_type){

		ir->data_type = left_type;
	}
	// check for array loads
	// this is any time we have a bracketed node on the right side,
	// OR on the left in an op that is NOT an assign.
	// do the right side first:
	else if(right_type & TYPE_ARRAY){

		right_type &= ~TYPE_ARRAY;

		if(right_type != left_type){

			printf("ERROR cannot do type conversion for: %d != %d\n", left_type, right_type);

			return -1;
		}

		ir->data_type = left_type;
	}
	else if(left_type & TYPE_ARRAY){

		ir->data_type = left_type;
		// apply vector status:
		ir->args.assign_op.is_vector = left.args.ident.is_vector;
	}
	else if((left_type == TYPE_FIXED16) && (right_type == TYPE_NUMBER)){

		// f16

		ir->data_type = left_type;
	}
	else if((left_type == TYPE_NUMBER) && (right_type == TYPE_FIXED16)){

		// f16

		ir->data_type = right_type;
	}
	else if((left_type == TYPE_GFX16) ||(right_type == TYPE_GFX16)){

		// gfx16
	}
	else if(right_type == TYPE_ANY){

		ir->data_type = left_type;
	}
	else{

		printf("ERROR cannot do type conversions yet! %d != %d\n", left_type, right_type);

		return -1;
	}

	ir->args.assign_op.left_type = left_type;
	ir->args.assign_op.right_type = right_type;

	return 0;
}

int pass_0_bracket(ir_state_t *ir_state, ir_t *ir){

	debug_printf("BRACKET\n");

	// bracket type is number
	ir->data_type = TYPE_NUMBER;

	int rank = ir->args.bracket.exprs;
	debug_printf("\trank: %d\n", rank);

	while(rank > 0){

		if(ir_pop_stack(ir_state, NULL) < 0){

			return -1;
		}

		rank--;
	}

	// check for ident
	if(ir[1].type == IR_IDENT){

		ir[1].args.ident.is_vector = false;
	}
	else if(ir[1].type == IR_LVALUE){

		ir[1].args.ident.is_vector = false;
	}	
	else{

		printf("Invalid bracket construction\n");

		return -1;
	}

	return 0;
}

int pass_0_dot(ir_state_t *ir_state, ir_t *ir){

	debug_printf("DOT\n");

	ir_print(ir);

	int r_index = ir_pop_stack(ir_state, NULL);

	if(r_index < 0){

		return -1;
	}

	int l_index = ir_pop_stack(ir_state, NULL);

	if(l_index < 0){

		return -1;
	}

	ir_t *ir_array = (ir_t *)array_get_data(ir_state->ir_list);

	ir_t *left = &ir_array[l_index];
	ir_t *right = &ir_array[r_index];

	ir_t *left1 = &ir_array[l_index - 1];
	ir_t *right1 = &ir_array[r_index - 1];


	debug_printf("\tleft: %d ", left->data_type);
	ir_print(left);

	debug_printf("\t\tleft1: %d ", left1->data_type);
	ir_print(left1);

	debug_printf("\tright: %d ", right->data_type);
	ir_print(right);
	
	debug_printf("\t\tright1: %d ", right1->data_type);
	ir_print(right1);

	
	ir->args.dot.obj_hash = left->args.ident.ident_hash;
	ir->args.dot.obj_rank = 0;

	if(left1->type == IR_BRACKET){

		ir->args.dot.obj_rank = left1->args.bracket.exprs;
	}
	
	ir->args.dot.field_hash = right->args.ident.ident_hash;
	ir->args.dot.field_rank = 0;

	if(right1->type == IR_BRACKET){

		ir->args.dot.field_rank = right1->args.bracket.exprs;
	}

	// lookup object
	obj_def_t obj_def = {0};

	int field_index = ir_lookup_obj_field(ir_state, ir->args.dot.obj_hash, ir->args.dot.field_hash, &obj_def);
	if(field_index < 0){

		printf("Object/field not found: %s.%s\n", hash_s_get_str(ir->args.dot.obj_hash), hash_s_get_str(ir->args.dot.field_hash));
		return -1;
	}
	
	// push to stack
	if(ir_push_stack(ir_state, ir) < 0){

		return -1;
	}

	if(right->type == IR_LVALUE){

		ir->args.dot.store = true;

		// annotate with type
		ir->data_type = TYPE_OBJECT;

		if(ir->args.dot.obj_hash == HASH32(kv)){

			right->args.ident.assume_resolved = true;
		}
	}
	else{

		ir->data_type = obj_def.fields[field_index].type;
	}

	return 0;
}

int pass_0_obj_field(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_obj_t *args = &ir->args.obj;

	debug_printf("OBJ_FIELD: %s.%s\n", hash_s_get_str(args->obj_hash), hash_s_get_str(args->field_hash));

	int index = ir_lookup_obj_def(ir_state, args->obj_hash, NULL);
	
	if(index < 0){

		printf("Object definition not found\n");

		return -1;
	}

	obj_def_t *obj_ptr = array_get_data(ir_state->objects);

	if(obj_ptr[index].field_count >= MAX_OBJECT_FIELDS){

		printf("Max object fields exceeded\n");

		return -1;
	}

	var_info_t var_info = {
		args->data_type,
		false, // is ref
		args->field_hash,
		0,
		index,
		-1,
		-1,
	};

	obj_ptr[index].fields[obj_ptr[index].field_count] = var_info;
	obj_ptr[index].field_count++;

	return 0;
}

int pass_0_call(ir_state_t *ir_state, ir_t *ir){

	ir_arg_call_t *args = &ir->args.call;

	debug_printf("CALL: %s args: %d\n", hash_s_get_str(args->ident_hash), args->arg_count);

	bool is_indirect = false;
	
	// look up function and get return type
	func_def_t func_def = {0};

	// check if there are function args
	if(args->arg_count > 0){

		// peek at first arg
		int arg0_index = ir_peek_stack(ir_state, NULL);

		ir_t *ir_array = (ir_t *)array_get_data(ir_state->ir_list);

		ir_t *ir_arg = &ir_array[arg0_index];

		// check if accessing KV object
		if((ir_arg->type == IR_DOT) && (ir_arg->args.dot.obj_hash == HASH32(kv))){

			if(args->ident_hash == HASH32(len)){

				// change name of function call for the KV version
				args->ident_hash = HASH32(_kvlen);

				// since this is a dot, we actually have two params on the stack even
				// though the function technically expects one.
				// set call to discard extra param
				args->discard = true;

				// change the IR DOT mode so the next pass does not emit
				// a load object instruction
				ir_arg->args.dot.store = true;				
			}
		}
	}
	

	if(ir_lookup_libfunc(ir_state, args->ident_hash, &func_def) >= 0){

		// check arg count
		if((args->arg_count < func_def.arg_count_min) || (args->arg_count > func_def.arg_count_max)){

			printf("Incorrect number of arguments\n");

			return -1;
		}
	}
	else if(ir_lookup_func(ir_state, args->ident_hash, &func_def) >= 0){

		if(args->arg_count != func_def.arg_count){

			printf("Incorrect number of arguments\n");

			return -1;
		}
	}
	else{

		// check if indirect
		data_type_t data_type = TYPE_NONE;

		ir_lookup_type(ir_state, args->ident_hash, &data_type);

		if(data_type != TYPE_REF_FUNCTION){

			printf("Function not found!\n");
			return -1;	
		}

		is_indirect = true;
	}

	// pop args
	for(int i = 0; i < args->arg_count; i++){

		ir_t arg;

		if(ir_pop_stack(ir_state, &arg) < 0){

			printf("Stack underflow on call arguments.\n");

			return -1;
		}

		// NOTE!!!!
		// This is where we type check function args!
		// but we need to record that information in func def.
		// verify passed type

	}

	if(is_indirect){

		debug_printf("indirect call!\n");
	}

	ir->data_type = func_def.return_type;
	debug_printf("call return type: %d\n", func_def.return_type);

	if(func_def.return_type != TYPE_NONE){

		// returning a value
		if(ir_push_stack(ir_state, ir) < 0){

			return -1;
		}
	}

	args->indirect = is_indirect;

	return 0;
}

int pass_0_return(ir_state_t *ir_state, ir_t *ir){

	debug_printf("RETURN\n");

	if(ir_state->func_index < 0){

		// cannot return in global context
		printf("Cannot return in global context\n");

		return -1;
	}

	debug_printf("%d %d\n", ir->args.ret.return_expressions, ir_stack_depth(ir_state));

	for(int i = 0; i < ir->args.ret.return_expressions; i++){

		if(ir_pop_stack(ir_state, NULL) < 0){

			return -1;
		}
	}

	return 0;
}

int pass_0_yield(ir_state_t *ir_state, ir_t *ir){

	debug_printf("YIELD\n");

	if(ir_state->func_index < 0){

		// cannot yield in global context
		printf("Cannot yield in global context\n");

		return -1;
	}

	// check stack
	if(ir_stack_depth(ir_state) > 0){

		printf("Cannot yield with unbalanced stack\n");

		return -1;
	}

	return 0;
}

int pass_0_delay(ir_state_t *ir_state, ir_t *ir){

	debug_printf("DELAY\n");

	if(ir_state->func_index < 0){

		// cannot delay in global context
		printf("Cannot delay in global context\n");

		return -1;
	}

	// consume value from stack
	if(ir_pop_stack(ir_state, NULL) < 0){

		return -1;
	}

	return 0;
}

int pass_0_rate(ir_state_t *ir_state, ir_t *ir){

	debug_printf("RATE\n");

	if(ir_state->func_index < 0){

		// cannot delay in global context
		printf("Cannot rate in global context\n");

		return -1;
	}

	// consume value from stack
	if(ir_pop_stack(ir_state, NULL) < 0){

		return -1;
	}

	return 0;
}

int pass_0_publish(ir_state_t *ir_state, ir_t *ir){

	debug_printf("PUBLISH: %s\n", hash_s_get_str(ir->args.ident.ident_hash));

	if(ir_state->func_index >= 0){

		// cannot delay in local context
		printf("Cannot publish in local context\n");

		return -1;
	}

	return 0;
}

int ir_get_globals_count(ir_state_t *ir_state){

	int count = 0;
	const var_info_t *ptr = array_get_data(ir_state->global_symbols);

	for(int i = 0; i < array_get_count(ir_state->global_symbols); i++){

		if(ptr->func_index < 0){

			count++;
		}

		ptr++;
	}

	return count;
}

int ir_get_locals_count(ir_state_t *ir_state){

	int count = 0;
	const var_info_t *ptr = array_get_data(ir_state->global_symbols);

	for(int i = 0; i < array_get_count(ir_state->global_symbols); i++){

		if(ptr->func_index >= 0){

			func_def_t func_def = {0};

			ir_lookup_func_by_index(ir_state, ptr->func_index, &func_def);

			if(!func_def.is_coroutine){

				count++;	
			}
		}

		ptr++;
	}

	return count;
}

int pass_0_func(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_func_t *args = &ir->args.func;

	if(args->is_coroutine){

		debug_printf("CORO: %s\n", hash_s_get_str(args->ident_hash));
	}
	else{

		debug_printf("FUNC: %s\n", hash_s_get_str(args->ident_hash));	
	}

	uint16_t locals_index = 0;

	if(!args->is_coroutine){

		locals_index = ir_get_locals_count(ir_state);
	}
	// NOTE!
	// coroutine locals start at 0!
	// the actual local index will be computed at run time.
	
	func_def_t func = {
		args->ident_hash,
		args->arg_count,
		args->arg_count,
		args->arg_count,
		// {},
		ir->data_type,
		args->label_index,
		locals_index,
		0,
		0,
		0,
		args->is_coroutine,
	};

	debug_printf("func return type: %d\n", func.return_type);

	ir_state->func_index = array_get_count(ir_state->functions);
	array_add(&ir_state->functions, &func);

	return 0;
}


int pass_0_end_func(ir_state_t *ir_state, ir_t *ir){

	debug_printf("END FUNC\n");

	// reset func index to global
	ir_state->func_index = -1;

	return 0;
}

int pass_0_jump(ir_state_t *ir_state, ir_t *ir){

	if(ir->args.jump.jump == IR_JUMP_ZERO){

		if(ir_pop_stack(ir_state, NULL) < 0){

			return -1;
		}
	}
	else if(ir->args.jump.jump == IR_JUMP_NOT_ZERO){

		if(ir_pop_stack(ir_state, NULL) < 0){

			return -1;
		}
	}	
	else if(ir->args.jump.jump == IR_JUMP_ALWAYS){
		
	}	
	else if(ir->args.jump.jump == IR_JUMP_BREAK){
		
		ir->args.jump.jump = IR_JUMP_ALWAYS;
	}	
	else if(ir->args.jump.jump == IR_JUMP_CONTINUE){
		
		ir->args.jump.jump = IR_JUMP_ALWAYS;
	}	
	else{

		printf("Invalid jump type!\n");

		return -1;
	}

	return 0;
}

int pass_0_loop_init(ir_state_t *ir_state, ir_t *ir){

	const ir_arg_loop_init_t *args = &ir->args.loop_init;

	debug_printf("LOOP_INIT: %s\n", hash_s_get_str(args->ident_hash));

	if(ir_state->func_index < 0){

		// for loop not allowed in global because we cannot create locals

		printf("for loops not allowed in global context\n");

		return -1;
	}
		
	// local var not found, create it
	if(ir_lookup_symbol(ir_state, args->ident_hash, NULL) < 0){

		if(ir_add_local_var(ir_state, args->ident_hash, TYPE_NUMBER, ir_state->func_index)){

			return -1;
		}

		var_info_t *var_ptr = 0;

		if(ir_lookup_symbol_ptr(ir_state, args->ident_hash, &var_ptr) < 0){

			return -1;
		}

		func_def_t func_def = {0};

		if(ir_lookup_func_by_index(ir_state, ir_state->func_index, &func_def) < 0){

			return -1;
		}

		// uint32_t zero = 0;

		if(func_def.is_coroutine){

			var_ptr->addr = func_def.locals_size;

			ir_add_func_local(ir_state, ir_state->func_index);
		}
		else{

			// set address
			var_ptr->addr = ir_state->locals_count - func_def.locals_index;
			
			// add to vm locals
			ir_state->locals_count++;
			// array_add(&ir_state->vm.locals_list, &zero);

			ir_add_func_local(ir_state, ir_state->func_index);
		}
	}
	
	return 0;
}

int pass_0_loop_exit(ir_state_t *ir_state, ir_t *ir){

	debug_printf("LOOP_EXIT\n");

	return 0;
}

int ir_handle_unused_call_values(ir_state_t *ir_state){

	debug_printf("Checking for unused call values\n");

	ir_t *ir_array = array_get_data(ir_state->ir_list);

	// look for unused call values
	for(int j = 0; j < ir_state->stack_index; j++){
					
		uint16_t index = ir_state->ir_stack[j];

		// check if call
		if(ir_array[index].type == IR_CALL){

			debug_printf("unused return\n");

			ir_array[index].args.call.unused_return = true;

			// remove from further consideration
			ir_state->ir_stack[j] = 0;
		}
	}

	return 0;
}


int pass_0_assert(ir_state_t *ir_state, ir_t *ir){

	debug_printf("ASSERT\n");

	if(ir_pop_stack(ir_state, NULL) < 0){

		return -1;
	}

	return 0;
}

int ir_pass_0(ir_state_t *ir_state){

	debug_printf("-----------------\n");
	debug_printf("IR PASS TYPING\n");


	// check label stacks, they should be empty!
	if(ir_state->label_entry_index != 0){

		printf("Unmatched entry labels!\n");

		return -1;
	}

	if(ir_state->label_exit_index != 0){

		printf("Unmatched exit labels!\n");

		return -1;
	}


	ir_t *ir = array_get_data(ir_state->ir_list);

	int max_stack_depth = 0;

	for(int i = 0; i < array_get_count(ir_state->ir_list); i++){

		// printf("Stack index: %d\n", ir_state->stack_index);

		// ir_print(ir);
		switch(ir->type){
			case IR_DECLARE:
				if(pass_0_declare(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_IDENT:
				if(pass_0_ident(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_LVALUE:
				if(pass_0_lvalue(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_ASSERT:
				if(pass_0_assert(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_END_EXPR:
				if(pass_0_end_expr(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_PUSH_CONST:
				if(pass_0_push_const(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_ASSIGN:
				if(pass_0_assign(ir_state, ir) < 0){

					return -1;
				}
			
				break;

			case IR_BINOP:
				if(pass_0_binop(ir_state, ir) < 0){

					return -1;
				}
			
				break;

			case IR_ASSIGN_OP:
				if(pass_0_assign_op(ir_state, ir) < 0){

					return -1;
				}
			
				break;

			case IR_BRACKET:
				if(pass_0_bracket(ir_state, ir) < 0){

					return -1;
				}
			
				break;

			case IR_DOT:
				if(pass_0_dot(ir_state, ir) < 0){

					return -1;
				}
			
				break;
				
			case IR_OBJ_FIELD:
				if(pass_0_obj_field(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_CALL:
				if(pass_0_call(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_RETURN:
				if(pass_0_return(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_YIELD:
				if(pass_0_yield(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_DELAY:
				if(pass_0_delay(ir_state, ir) < 0){

					return -1;
				}

				break;
			
			case IR_RATE:
				if(pass_0_rate(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_PUBLISH:
				if(pass_0_publish(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_FUNC:
				if(pass_0_func(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_END_FUNC:
				if(pass_0_end_func(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_JUMP:
				if(pass_0_jump(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_LOOP_INIT:
				if(pass_0_loop_init(ir_state, ir) < 0){

					return -1;
				}

				break;

			case IR_LOOP_EXIT:
				if(pass_0_loop_exit(ir_state, ir) < 0){

					return -1;
				}

				break;

			default:
				break;
		}

		if(ir_state->stack_index > max_stack_depth){

			max_stack_depth = ir_state->stack_index;
		}

		if(ir_state->stack_index == 0){

			ir->stack_empty = true;
			// printf("------ stack empty\n");
		}
		else{

			// printf("------ stack depth: %d\n", ir_state->stack_index);
		}

		ir++;
	}

	ir_handle_unused_call_values(ir_state);

	debug_printf("Max stack depth: %d\n", max_stack_depth);

	ir_t *ir_array = array_get_data(ir_state->ir_list);

	if(ir_state->stack_index > 0){

		int unbalanced_count = 0;

		for(int j = 0; j < ir_state->stack_index; j++){

			debug_printf("\t");

			uint16_t index = ir_state->ir_stack[j];

			if(index == 0){

				continue;
			}

			unbalanced_count++;

			debug_printf("Unused value from line: %d\t", ir_array[index].line_num);

			ir_print(&ir_array[index]);
		}

		if(unbalanced_count > 0){

			printf("Unbalanced IR stack! %d extra items\n", unbalanced_count);

			return -1;
		}
	}
	else{

		debug_printf("~~~ IR stack is balanced ~~~\n");
	}

	debug_printf("END TYPING PASS\n");
	debug_printf("-----------------\n");

	return 0;
}


#endif