#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "mygale.h"
#include "Zend/zend_operators.h"

/* Pointer to the original execute function */
static ZEND_DLEXPORT void (*_zend_execute) (zend_op_array *ops TSRMLS_DC);
static char *hp_get_function_name(zend_op_array *ops TSRMLS_DC);
static zval *get_current_args (zend_op_array *ops TSRMLS_DC);

#ifndef Z_ADDREF_P
# define Z_ADDREF_P ZVAL_ADDREF
#endif

/*** FUNCTION DECLARATION ***/
/* La liste des fonctions que zend va rendre accessible par ce module */
static zend_function_entry mygale_functions[] =
{
    ZEND_FE(mygale_start, NULL)
    ZEND_FE(mygale_add_before, NULL)
    ZEND_FE(mygale_add_around, NULL)
    ZEND_FE(mygale_stop, NULL)
    {NULL, NULL, NULL}
};

/* Informations sur notre module 
    On commence simple, on ne donne que des entetes standards
    un nom de module et la liste des méthodes...
    On laisse 5 valeurs à null (vous verrez plus tard)
    on donne une version et des propriétés par défaut
*/
zend_module_entry mygale_module_entry = 
{
    STANDARD_MODULE_HEADER,
    PHP_MYGALE_EXTNAME,
    mygale_functions,
    NULL, 
    NULL, 
    NULL, 
    NULL, 
    NULL,
    PHP_MYGALE_VERSION,
    STANDARD_MODULE_PROPERTIES
};


static zend_class_entry* mygalepc_class_entry;

zend_object_handlers mygalepc_object_handlers;

typedef struct {
	zval *callback;
	zval *selector;
} pointcut;

typedef struct {
	zval *callback;
	zval *object;
} ipointcut;

typedef struct {
    zend_object std;
    zval *args;
    char *funcName;
    zend_op_array *op;
    zend_execute_data *ex;
    zval *this;
    zval ***ret;
    ipointcut *ipointcut;
}  mygalepc_object;


PHP_METHOD(MygalePC, getArgs);
PHP_METHOD(MygalePC, getFunctionName);
PHP_METHOD(MygalePC, process);

ZEND_BEGIN_ARG_INFO(arginfo_mygalepc_getargs, 0)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO(arginfo_mygalepc_getfunctionname, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_mygalepc_process, 0)
ZEND_END_ARG_INFO()

zval *new_execute(ipointcut *);

static const zend_function_entry mygalepc_functions[] = {
        PHP_ME(MygalePC, getArgs,arginfo_mygalepc_getargs, 0)
        PHP_ME(MygalePC, process,arginfo_mygalepc_process, 0)
        PHP_ME(MygalePC, getFunctionName,arginfo_mygalepc_getfunctionname, 0)
	{NULL, NULL, NULL}
};

PHP_METHOD(MygalePC, process)
{
	mygalepc_object *obj = (mygalepc_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	if (obj->ipointcut==NULL) {
	  	zend_execute_data *prev_data;
		zval **original_return_value;	
		zend_op **original_opline_ptr;
		zend_op_array *prev_op;
	        HashTable *calling_symbol_table;
		zval *prev_this;

		//Save previous context
		prev_op = EG(active_op_array);
  		prev_data = EG(current_execute_data);
                original_opline_ptr = EG(opline_ptr);
                calling_symbol_table = EG(active_symbol_table);
		prev_this = EG(This);
		
		EG(active_op_array) = (zend_op_array *) obj->op;
		EG(current_execute_data) = obj->ex;
		EG(This) = obj->this;

                _zend_execute(EG(active_op_array) TSRMLS_CC);
		
		//Take previous context
		EG(This) = prev_this;
                EG(opline_ptr) = original_opline_ptr;
		EG(active_op_array) = (zend_op_array *) prev_op;
		EG(current_execute_data) = prev_data;
                EG(active_symbol_table) = calling_symbol_table;
// zend_throw_exception(zend_exception_get_default(TSRMLS_C), "String could not be parsed as XML", 0 TSRMLS_CC);
//return;
		//Only if we do not have exception
		if (!EG(exception)) {
			COPY_PZVAL_TO_ZVAL(*return_value,(zval *)*EG(return_value_ptr_ptr));
		} else {


		}
		
	} else {
		zval *exec_return;
		//php_printf("BEFORE EXE");
		exec_return = new_execute(obj->ipointcut);
		//php_printf("AFTER EXE");

		//Only if we do not have exception
		if (!EG(exception)) {
			COPY_PZVAL_TO_ZVAL(*return_value, exec_return);
		} 
	}

	//return;
}

PHP_METHOD(MygalePC, getArgs)
{
	mygalepc_object *obj = (mygalepc_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

	Z_TYPE_P(return_value)	 = IS_ARRAY;
	Z_ARRVAL_P(return_value) = Z_ARRVAL_P(obj->args);
	return;
}

PHP_METHOD(MygalePC, getFunctionName)
{
	mygalepc_object *obj = (mygalepc_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

	Z_TYPE_P(return_value)	 = IS_STRING;
	Z_STRVAL_P(return_value) = obj->funcName;
	Z_STRLEN_P(return_value) = strlen(obj->funcName);
	return;
}


void mygalepc_free_storage(void *object TSRMLS_DC)
{
    mygalepc_object *obj = (mygalepc_object *)object;
    zend_hash_destroy(obj->std.properties);
    FREE_HASHTABLE(obj->std.properties);
    efree(obj);
}




zend_object_value mygalepc_create_handler(zend_class_entry *type TSRMLS_DC)
{
    zval *tmp;
    zend_object_value retval;

    mygalepc_object *obj = (mygalepc_object *)emalloc(sizeof(mygalepc_object));
    memset(obj, 0, sizeof(mygalepc_object));
    obj->std.ce = type;

    ALLOC_HASHTABLE(obj->std.properties);
    zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
    //zend_hash_copy(obj->std.properties, &type->default_properties,
     //   (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));

    retval.handle = zend_objects_store_put(obj, NULL,
        mygalepc_free_storage, NULL TSRMLS_CC);
    retval.handlers = &mygalepc_object_handlers;

    return retval;
}
#if COMPILE_DL_MYGALE
ZEND_GET_MODULE(mygale) 
#endif

ZEND_DLEXPORT void mygale_execute (zend_op_array *ops TSRMLS_DC);

int started = 0;

zval *before_arr;
zval *around_arr;

zval* execute_please (zval func, char *name, zval *call_args, zend_op_array *ops, zval *callback) {
        zval *object;
        zval *args[1];
        MAKE_STD_ZVAL(object);
        Z_TYPE_P(object) = IS_OBJECT;
        (object)->value.obj = mygalepc_create_handler(mygalepc_class_entry);
        mygalepc_object *obj = (mygalepc_object *)zend_object_store_get_object(object TSRMLS_CC);
        obj->args = call_args;
        obj->funcName = name;
        if (callback!=NULL) {
        } else {
                obj->op = ops;
                obj->ex = EG(current_execute_data);
        }
        args[0] = &object;
        zval *zret_ptr=NULL;
        zend_fcall_info fci;
        zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
        TSRMLS_FETCH();
        fci.param_count= 1;
        fci.size= sizeof(fci);
        fci.function_table= EG(function_table);
        fci.function_name= &func;
        fci.symbol_table= NULL;
        fci.retval_ptr_ptr= &zret_ptr;
        fci.params = args;
        fci.object_ptr= NULL;
        fci.no_separation= 0;
                if (zend_call_function(&fci, &fcic TSRMLS_CC) == FAILURE) {
                       //php_printf("BUG [ %s ]\n", name);
                }
        return zret_ptr;
}

void start_mygale () {
	if (!started) {
		started=1;		
	        zend_class_entry ce;

		INIT_CLASS_ENTRY(ce, "MygalePC", mygalepc_functions);
		mygalepc_class_entry = zend_register_internal_class(&ce TSRMLS_CC);

		mygalepc_class_entry->create_object = mygalepc_create_handler;
		memcpy(&mygalepc_object_handlers,
			zend_get_std_object_handlers(), sizeof(zend_object_handlers));
		mygalepc_object_handlers.clone_obj = NULL;

		MAKE_STD_ZVAL(before_arr);
		array_init(before_arr);

		MAKE_STD_ZVAL(around_arr);
		array_init(around_arr);

		_zend_execute = zend_execute;
		zend_execute = mygale_execute;
	}
}

zval *new_execute (ipointcut *pc) {
	zval *args[1];
	args[0] = (zval *)&(pc->object);
	zval *zret_ptr=NULL;
        zend_fcall_info fci;
        zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
        TSRMLS_FETCH();
        fci.param_count= 1;
        fci.size= sizeof(fci);
        fci.function_table= EG(function_table);
        fci.function_name= pc->callback;
        fci.symbol_table= NULL;
        fci.retval_ptr_ptr= &zret_ptr;
        fci.params = (zval ***)args;
        fci.object_ptr= NULL;
        fci.no_separation= 0;
        if (zend_call_function(&fci, &fcic TSRMLS_CC) == FAILURE) {
       	//php_printf("BUG\n");
       	}
	if (!EG(exception)) {
		//php_printf("AFTER EXECUTE\n");
		return zret_ptr;
	}

}



int compare_selector (char *str1, char *str2) {
	return !strcmp(str1,str2);
}

ZEND_DLEXPORT void mygale_execute (zend_op_array *ops TSRMLS_DC) {


	char          *func = NULL;
	func = hp_get_function_name(ops TSRMLS_CC);
	
	zval *args=NULL;
	args = get_current_args(ops);

	HashPosition pos;
	pointcut **temppc;
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(before_arr),&pos);
	while (zend_hash_get_current_data_ex(Z_ARRVAL_P(before_arr), (void **)&temppc, &pos)==SUCCESS) {
		zend_hash_move_forward_ex(Z_ARRVAL_P(before_arr), &pos);
	
		if (compare_selector(Z_STRVAL_P((*temppc)->selector),func)) {
			zval *rtr=NULL;
			rtr = execute_please (*(*temppc)->callback, func, args, NULL, NULL);
		}
	}
	
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(around_arr),&pos);
	int arounded = 0;
	ipointcut *previous_ipc = NULL;

	zval ***ret_test;
	while (zend_hash_get_current_data_ex(Z_ARRVAL_P(around_arr), (void **)&temppc, &pos)==SUCCESS) {
		zend_hash_move_forward_ex(Z_ARRVAL_P(around_arr), &pos);
	
		if (compare_selector(Z_STRVAL_P((*temppc)->selector),func)) {
			arounded=1;
			zval *rtr=NULL;

			zval *object;
			MAKE_STD_ZVAL(object);
			Z_TYPE_P(object) = IS_OBJECT;
			(object)->value.obj = mygalepc_create_handler(mygalepc_class_entry);
			mygalepc_object *obj = (mygalepc_object *)zend_object_store_get_object(object TSRMLS_CC);
			obj->args = args;
			zval_copy_ctor(obj->args);
			obj->funcName = emalloc(sizeof(char *)*strlen(func));
			strcpy(obj->funcName,func);
			if (previous_ipc!=NULL) {
				obj->ipointcut = previous_ipc;
			} else {
				obj->op = ops;
				obj->ex = EG(current_execute_data);
				ret_test = &EG(return_value_ptr_ptr);
				obj->this = EG(This);
			}
			previous_ipc = emalloc(sizeof(ipointcut));
			previous_ipc->callback = (*temppc)->callback;
			previous_ipc->object = object;
		}
	}
	if (!arounded) {
		_zend_execute(ops TSRMLS_CC);
	} else {
		zval *exec_return;
		exec_return = new_execute(previous_ipc);
		//Only if we do not have exception
		if (!EG(exception)) {
	                if (EG(return_value_ptr_ptr)) {
				*EG(return_value_ptr_ptr)=exec_return;
			}
		}

	}
	return;
}



ZEND_FUNCTION(mygale_add_around)
{
	start_mygale();
	pointcut *PC = emalloc(sizeof(pointcut));
	zval *callback;
	zval *selector;
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &selector, &callback) == FAILURE) {
                return;
        }
	Z_ADDREF_P(callback);
	Z_ADDREF_P(selector);
	PC->selector = selector;
	PC->callback = callback;	

	zend_hash_next_index_insert(around_arr->value.ht, &PC, sizeof(pointcut *), NULL);

}
ZEND_FUNCTION(mygale_add_before)
{
	start_mygale();

	pointcut *PC = emalloc(sizeof(pointcut));
	zval *callback;
	zval *selector;
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &selector, &callback) == FAILURE) {
                return;
        }
	Z_ADDREF_P(callback);
	Z_ADDREF_P(selector);
	PC->selector = selector;
	PC->callback = callback;	

	zend_hash_next_index_insert(before_arr->value.ht, &PC, sizeof(pointcut *), NULL);

}

ZEND_FUNCTION(mygale_start)
{
	start_mygale();
}

ZEND_FUNCTION(mygale_stop)
{
	zend_execute = _zend_execute;
}


static char *hp_get_function_name(zend_op_array *ops TSRMLS_DC) {
  zend_execute_data *data;
  char              *func = NULL;
  const char        *cls = NULL;
  char              *ret = NULL;
  int                len;
  zend_function      *curr_func;

  data = EG(current_execute_data);

  if (data) {
    /* shared meta data for function on the call stack */
    curr_func = data->function_state.function;

    /* extract function name from the meta info */
    func = curr_func->common.function_name;

    if (func) {
      /* previously, the order of the tests in the "if" below was
       * flipped, leading to incorrect function names in profiler
       * reports. When a method in a super-type is invoked the
       * profiler should qualify the function name with the super-type
       * class name (not the class name based on the run-time type
       * of the object.
       */
      if (curr_func->common.scope) {
        cls = curr_func->common.scope->name;
      } else if (data->object) {
        cls = Z_OBJCE(*data->object)->name;
      }

      if (cls) {
        len = strlen(cls) + strlen(func) + 10;
        ret = (char*)emalloc(len);
        snprintf(ret, len, "%s::%s", cls, func);
      } else {
        ret = estrdup(func);
      }
    } else {
      long     curr_op;
      int      desc_len;
      char    *desc;
      int      add_filename = 0;

      /* we are dealing with a special directive/function like
       * include, eval, etc.
       */
      curr_op = data->opline->extended_value;

      switch (curr_op) {
        case ZEND_EVAL:
          func = "eval";
          break;
        case ZEND_INCLUDE:
          func = "include";
          add_filename = 1;
          break;
        case ZEND_REQUIRE:
          func = "require";
          add_filename = 1;
          break;
        case ZEND_INCLUDE_ONCE:
          func = "include_once";
          add_filename = 1;
          break;
        case ZEND_REQUIRE_ONCE:
          func = "require_once";
          add_filename = 1;
          break;
        default:
          func = "???_op";
          break;
      }

      /* For some operations, we'll add the filename as part of the function
       * name to make the reports more useful. So rather than just "include"
       * you'll see something like "run_init::foo.php" in your reports.
       */
      if (add_filename){
        char *filename;
        int   len;
        filename = (char *)hp_get_base_filename((curr_func->op_array).filename);
        len      = strlen("run_init") + strlen(filename) + 3;
        ret      = (char *)emalloc(len);
        snprintf(ret, len, "run_init::%s", filename);
      } else {
        ret = estrdup(func);
      }
    }
  }
  return ret;
}


static zval *get_current_args (zend_op_array *ops TSRMLS_DC) {
	void **p;
        int arg_count;
        int i;
	zval *return_value;
	MAKE_STD_ZVAL(return_value);
	array_init(return_value);	
	zend_execute_data *ex = EG(current_execute_data);
	if (!ex || !ex->function_state.arguments) {
                zend_error(E_WARNING, "ooops");
                return;
        }

        p = ex->function_state.arguments;
        arg_count = (int)(zend_uintptr_t) *p;           /* this is the amount of arguments passed to func_get_args(); */

        for (i=0; i<arg_count; i++) {
                zval *element;

                ALLOC_ZVAL(element);
                *element = **((zval **) (p-(arg_count-i)));
                zval_copy_ctor(element);
                INIT_PZVAL(element);
                zend_hash_next_index_insert(return_value->value.ht, &element, sizeof(zval *), NULL);
        }
	return return_value;

}

