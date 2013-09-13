
#include <sys/types.h>
#include <regex.h>
#include <string.h>

#include <talloc/talloc.h>

#include "grub2.h"

struct env_entry {
	const char		*name;
	const char		*value;
	struct list_item	list;
};

static const char *env_lookup(struct grub2_script *script,
		const char *name, int name_len)
{
	struct env_entry *entry;
	const char *str;

	str = talloc_strndup(script, name, name_len);
	printf("%s: %s\n", __func__, str);

	list_for_each_entry(&script->environment, entry, list)
		if (!strncmp(entry->name, name, name_len))
			return entry->value;

	return NULL;
}

static bool expand_word(struct grub2_script *script, struct grub2_word *word)
{
	const char *val, *src;
	char *dest = NULL;
	regmatch_t match;
	int n;

	src = word->text;

	n = regexec(&script->var_re, src, 1, &match, 0);
	if (n == 0)
		return false;

	val = env_lookup(script, src + match.rm_so,
				 match.rm_eo - match.rm_so);
	if (val)
		val = "";

	dest = talloc_strndup(script, src, match.rm_so);
	dest = talloc_asprintf_append(dest, "%s%s", val, src + match.rm_eo);

	word->text = dest;
	return true;
}

/* iterate through the words in an argv, looking for expansions. If a
 * word is marked with expand == true, then we process any variable
 * substitutions.
 *
 * Once that's done, we may (if split == true) have to split the word to create
 * new argv items
 */
static void process_expansions(struct grub2_script *script,
		struct grub2_argv *argv)
{
	struct grub2_word *word;

	list_for_each_entry(&argv->words, word, argv_list) {
		if (!word->expand)
			continue;

		expand_word(script, word);
	}
}

static int script_destroy(void *p)
{
	struct grub2_script *script = p;
	regfree(&script->var_re);
	return 0;
}

struct grub2_script *create_script(void *ctx)
{
	struct grub2_script *script;
	int rc;

	script = talloc(ctx, struct grub2_script);

	rc = regcomp(&script->var_re,
		"\\$\\{?([[:alpha:]][_[:alnum:]]*|[0-9]|[\\?@\\*#])\\}?",
			REG_EXTENDED);
	if (rc) {
		char err[200];
		regerror(rc, &script->var_re, err, sizeof(err));
		fprintf(stderr, "RE error %d: %s\n", rc, err);
		talloc_free(script);
		return NULL;

	}
	talloc_set_destructor(script, script_destroy);

	list_init(&script->environment);

	return script;
}
