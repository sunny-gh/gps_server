// package_locale.h: локализация 

//#define GETTEXT_PACKAGE "drawmap"
#define PACKAGE_LOCALE_DIR "../share/locale"

#include "config.h"
//#include <glib/gi18n.h>
#include <glib/gi18n-lib.h> // определение для _(), N_(), C_(), NC_()

void locale_init(void);

// удаление символов '_' в строке, новая строка требует удаления
gchar* del_(const gchar *str);