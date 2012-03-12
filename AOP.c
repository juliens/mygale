#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "AOP.h"
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
static zend_function_entry AOP_functions[] =
{
    ZEND_FE(AOP_start, NULL)
    ZEND_FE(AOP_add_before, NULL)
    ZEND_FE(AOP_add_around, NULL)
    ZEND_FE(AOP_stop, NULL)
    {NULL, NULL, NULL}
};

/* Informations sur notre module 
    On commence simple, on ne donne que des entetes standards
    un nom de module et la liste des méthodes...
    On laisse 5 valeurs à null (vous verrez plus tard)
    on donne une version et des propriétés par défaut
*/
zend_module_entry AOP_module_entry = 
{
    STANDARD_MODULE_HEADER,
    PHP_AOP_EXTNAME,
    AOP_functions,
    NULL, 
    NULL, 
    NULL, 
    NULL, 
    NULL,
    PHP_AOP_VERSION,
    STANDARD_MODULE_PROPERTIES
};


static zend_class_entry* AOPpc_class_entry;

zend_object_handlers AOPpc_object_handlers;

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
    int argsOverload;
}  AOPpc_object;


PHP_METHOD(AOPPC, getArgs);
PHP_METHOD(AOPPC, getThis);
PHP_METHOD(AOPPC, getFunctionName);
PHP_METHOD(AOPPC, process);
PHP_METHOD(AOPPC, processWithArgs);

ZEND_BEGIN_ARG_INFO(arginfo_AOPpc_getargs, 0)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO(arginfo_AOPpc_getfunctionname, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_AOPpc_process, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_AOPpc_getthis, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_AOPpc_processwithargs, 0)
ZEND_END_ARG_INFO()

zval *new_execute(ipointcut *);

static const zend_function_entry AOPpc_functions[] = {
        PHP_ME(AOPPC, getArgs,arginfo_AOPpc_getargs, 0)
        PHP_ME(AOPPC, getThis,arginfo_AOPpc_getthis, 0)
        PHP_ME(AOPPC, process,arginfo_AOPpc_process, 0)
        PHP_ME(AOPPC, processWithArgs,arginfo_AOPpc_processwithargs, 0)
        PHP_ME(AOPPC, getFunctionName,arginfo_AOPpc_getfunctionname, 0)
    {NULL, NULL, NULL}
};

zval *exec(AOPpc_object *obj, zval *args) {
    if (args!=NULL) {
        obj->argsOverload=1;
        obj->args = args;
    }
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
        if (obj->argsOverload) {
            int i,arg_count;
            zend_execute_data *ex = EG(current_execute_data);
            arg_count = (int)(zend_uintptr_t) *(ex->function_state.arguments);
            HashPosition pos;
            i=0;
            zval ** temp=NULL;
            zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(obj->args), &pos);

            while (zend_hash_get_current_data_ex(Z_ARRVAL_P(obj->args), (void **)&temp, &pos)==SUCCESS) {
               i++;
               zend_vm_stack_push_nocheck(*temp TSRMLS_CC);
               zend_hash_move_forward_ex(Z_ARRVAL_P(obj->args), &pos);
            }
            ex->function_state.arguments = zend_vm_stack_top(TSRMLS_C);
            zend_vm_stack_push_nocheck((void*)(zend_uintptr_t)i TSRMLS_CC);
        }

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
           return (zval *)*EG(return_value_ptr_ptr);
        } else {


        }
        
    } else {
        zval *exec_return;
        //php_printf("BEFORE EXE");
        if (obj->argsOverload) {
            AOPpc_object *objN = (AOPpc_object *)zend_object_store_get_object(obj->ipointcut->object TSRMLS_CC);
            objN->argsOverload=1;
            objN->args = obj->args;
            zval_copy_ctor(objN->args);
        }
        exec_return = new_execute(obj->ipointcut);
        //php_printf("AFTER EXE");

        //Only if we do not have exception
        if (!EG(exception)) {
            return exec_return;
        } 
    }

    return NULL;

}

PHP_METHOD(AOPPC, processWithArgs)
{
    AOPpc_object *obj = (AOPpc_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    zval *params;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &params) == FAILURE) {
        zend_error(E_ERROR, "Problem in processWithArgs");
    }
    zval *toReturn;
    toReturn = exec(obj, params);
    if (toReturn != NULL) {
            COPY_PZVAL_TO_ZVAL(*return_value, toReturn);
    }
    
}


PHP_METHOD(AOPPC, process)
{
    zval *toReturn;
    AOPpc_object *obj = (AOPpc_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    toReturn = exec(obj, NULL);
    if (toReturn != NULL) {
            COPY_PZVAL_TO_ZVAL(*return_value, toReturn);
    }
}

PHP_METHOD(AOPPC, getArgs)
{
    AOPpc_object *obj = (AOPpc_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

    Z_TYPE_P(return_value)   = IS_ARRAY;
    Z_ARRVAL_P(return_value) = Z_ARRVAL_P(obj->args);
	Z_ADDREF_P(return_value);
    return;
}

PHP_METHOD(AOPPC, getThis)
{
    AOPpc_object *obj = (AOPpc_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    if (obj->this!=NULL) {
        Z_TYPE_P(return_value)   = IS_OBJECT;
        Z_OBJVAL_P(return_value) = Z_OBJVAL_P(obj->this);
        //In order not to destroy OBJVAL(obj->this) 
        //Need to verify memory leak
        Z_ADDREF_P(return_value);
    }
    return;
}

PHP_METHOD(AOPPC, getFunctionName)
{
    AOPpc_object *obj = (AOPpc_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

    Z_TYPE_P(return_value)   = IS_STRING;
    Z_STRVAL_P(return_value) = obj->funcName;
    Z_STRLEN_P(return_value) = strlen(obj->funcName);
    return;
}


void AOPpc_free_storage(void *object TSRMLS_DC)
{
    AOPpc_object *obj = (AOPpc_object *)object;
    zend_hash_destroy(obj->std.properties);
    FREE_HASHTABLE(obj->std.properties);
    efree(obj);
}




zend_object_value AOPpc_create_handler(zend_class_entry *type TSRMLS_DC)
{
    zval *tmp;
    zend_object_value retval;

    AOPpc_object *obj = (AOPpc_object *)emalloc(sizeof(AOPpc_object));
    memset(obj, 0, sizeof(AOPpc_object));
    obj->std.ce = type;

    ALLOC_HASHTABLE(obj->std.properties);
    zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
    //zend_hash_copy(obj->std.properties, &type->default_properties,
     //   (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));

    retval.handle = zend_objects_store_put(obj, NULL,
        AOPpc_free_storage, NULL TSRMLS_CC);
    retval.handlers = &AOPpc_object_handlers;

    return retval;
}
#if COMPILE_DL_AOP
ZEND_GET_MODULE(AOP) 
#endif

ZEND_DLEXPORT void AOP_execute (zend_op_array *ops TSRMLS_DC);

int started = 0;

zval *before_arr;
zval *around_arr;

zval* execute_please (zval func, char *name, zval *call_args, zend_op_array *ops, zval *callback) {
        zval *object;
        zval *args[1];
        MAKE_STD_ZVAL(object);
        Z_TYPE_P(object) = IS_OBJECT;
        (object)->value.obj = AOPpc_create_handler(AOPpc_class_entry);
        AOPpc_object *obj = (AOPpc_object *)zend_object_store_get_object(object TSRMLS_CC);
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

void start_AOP () {
    if (!started) {
        started=1;      
            zend_class_entry ce;

        INIT_CLASS_ENTRY(ce, "AOPPC", AOPpc_functions);
        AOPpc_class_entry = zend_register_internal_class(&ce TSRMLS_CC);

        AOPpc_class_entry->create_object = AOPpc_create_handler;
        memcpy(&AOPpc_object_handlers,
            zend_get_std_object_handlers(), sizeof(zend_object_handlers));
        AOPpc_object_handlers.clone_obj = NULL;

        MAKE_STD_ZVAL(before_arr);
        array_init(before_arr);

        MAKE_STD_ZVAL(around_arr);
        array_init(around_arr);

        _zend_execute = zend_execute;
        zend_execute = AOP_execute;
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


int instance_of (char *str1, char *str2) {
        zend_class_entry **ce1; 
        zend_class_entry **ce2;
    //php_printf("TEST CLASS : %s = %s\n",str1,str2);
     if (zend_lookup_class(str1, strlen(str1), &ce1 TSRMLS_CC) == FAILURE) {
        php_printf("FAIL %s\n",str1);
        return 0;       
     }
     if (zend_lookup_class(str2, strlen(str2), &ce2 TSRMLS_CC) == FAILURE) {
        php_printf("FAIL %s\n",str2);
        return 0;
     }
    return instanceof_function(*ce1, *ce2 TSRMLS_CC);
}

char * substr (char *str,int start, int end) {
    if (start>strlen(str)) {
        return NULL;
    }
    if (end>strlen(str)) {
        end = strlen(str);
    }
    char *tmp = ecalloc(end-start+1, 1);
    strncat(tmp, str+start, end-start);
    return tmp;
}

char* get_class_part (char *str) {
    char *endp;
    char *class_end;
    endp=str+strlen(str);
    class_end = php_memnstr(str, "::", 2, endp);
    if (class_end!=NULL) {
        return substr(str,0,strlen(str)-strlen(class_end));

    }
    return NULL;
    
}

char * get_method_part (char *str) {

    char *endp;
    endp=str+strlen(str);
    return (php_memnstr(str, "::", 2, endp)+2);
}

int strcmp_with_joker (char *str_with_jok, char *str) {
    int i;
    int joker=0;
    for (i=0;i<strlen(str_with_jok);i++) {
        if (str_with_jok[i]=='*') {
            joker=i;
            break;
        }
    }
    if (joker) {
        return !strcmp(substr(str_with_jok,0,joker),substr(str,0,joker));
    } else {
        return !strcmp(str_with_jok,str);
    }
    
}

int compare_selector (char *str1, char *str2) {
    char *class1 = get_class_part(str1);
    char *class2 = get_class_part(str2);
    // No class so simple comp
    if (class1==NULL && class2==NULL) {
        return strcmp_with_joker(str1,str2);
    }
    // Only one with class => false
    if ((class1!=NULL && class2==NULL) || (class1==NULL && class2!=NULL)) {
        return 0;
    }
    
    //Two different classes => false
    if (class1!=NULL && class2!=NULL && !instance_of(class2,class1)) {
        return 0;
    }
    //Method only joker
    if (!strcmp (get_method_part(str1), "*")) {
        return 1;
    }
    return strcmp_with_joker(get_method_part(str1),get_method_part(str2));
}

ZEND_DLEXPORT void AOP_execute (zend_op_array *ops TSRMLS_DC) {


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
            (object)->value.obj = AOPpc_create_handler(AOPpc_class_entry);
            AOPpc_object *obj = (AOPpc_object *)zend_object_store_get_object(object TSRMLS_CC);
            obj->args = args;
            obj->argsOverload = 0;
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



ZEND_FUNCTION(AOP_add_around)
{
    start_AOP();
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
ZEND_FUNCTION(AOP_add_before)
{
    start_AOP();

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

ZEND_FUNCTION(AOP_start)
{
    start_AOP();
}

ZEND_FUNCTION(AOP_stop)
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
//        cls = curr_func->common.scope->name;
//      } else if (data->object) {
        cls = Z_OBJCE(*data->object)->name;
      }

      if (cls) {
        len = strlen(cls) + strlen(func) + 10;
        ret = (char*)emalloc(len);
        snprintf(ret, len, "%s::%s", cls, func);
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

