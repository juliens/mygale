#ifndef PHP_AOP_H
#define PHP_AOP_H 1

#define PHP_AOP_VERSION "0.1b"
#define PHP_AOP_EXTNAME "AOP"

/* déclaration de fonction à créer */
ZEND_FUNCTION(AOP_start);
ZEND_FUNCTION(AOP_add_before);
ZEND_FUNCTION(AOP_add_around);
ZEND_FUNCTION(AOP_stop);

/*Il nous faut aussi enregistrer les entrée de module*/
extern zend_module_entry AOP_module_entry;
#define phpext_AOP_ptr &AOP_module_entry

#endif
 
