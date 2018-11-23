// package_locale.c: локализация 

#ifdef _WIN32
#define inline __inline 
#endif

#include <glib.h>
#include <locale.h>
#include <libintl.h>
#include "package_locale.h"

//#include <gtkspell/gtkspell-win32.h>
const gchar *get_win32_prefix(void);
const gchar *get_win32_localedir(void) {
	return PACKAGE_LOCALE_DIR;
}

void locale_init(void) {
	const char *locale_dir = get_win32_localedir();
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, locale_dir);
	bindtextdomain("gtkspell3", locale_dir);
	bindtextdomain("webkitgtk-3.0", locale_dir);
	bindtextdomain("glib20", locale_dir);
	bindtextdomain("gtk30", locale_dir);

	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	bind_textdomain_codeset("gtkspell3", "UTF-8");
	bind_textdomain_codeset("webkitgtk-3.0", "UTF-8");
	bind_textdomain_codeset("webkit-1", "UTF-8");
	bind_textdomain_codeset("glib20", "UTF-8");
	bind_textdomain_codeset("gtk30", "UTF-8");
}

// удаление символов '_' в строке, новая строка требует удаления
gchar* del_(const gchar *str) {
	gchar *str1 = g_strdup(str);
	int len = (int) strlen(str1);
	int i, j;
	//g_free(str);
	for (i = 0; i < len; i++) {
		if (*(str1 + i) == '_') {
			for (j = i; j < len - 1; j++)
				*(str1 + j) = *(str1 + j + 1);
			// строка уменьшается на один байт
			*(str1 + len - 1) = 0;
			len--;
		}
	}
	//g_free(str);
	return str1;
}
