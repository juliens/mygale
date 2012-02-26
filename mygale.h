#ifndef PHP_MYGALE_H
#define PHP_MYGALE_H 1

#define PHP_MYGALE_VERSION "0.1b"
#define PHP_MYGALE_EXTNAME "mygale"

/* déclaration de fonction à créer */
ZEND_FUNCTION(mygale_start);
ZEND_FUNCTION(mygale_add_before);
ZEND_FUNCTION(mygale_add_around);
ZEND_FUNCTION(mygale_stop);

/*Il nous faut aussi enregistrer les entrée de module*/
extern zend_module_entry mygale_module_entry;
#define phpext_mygale_ptr &mygale_module_entry

#endif
 
