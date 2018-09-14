/*
   Copyright (C) 2004 - 2018 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)

   This file is part of MAQAO.

  MAQAO is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 3
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * \file
 * */
#include <stdlib.h>
#include <stdio.h>

#include "libmcommon.h"

/*
 * Initialize an help_t* object
 * \return an initialized help_t object
 */
help_t* help_initialize()
{
   help_t* help = lc_malloc0(sizeof(help_t));

   return (help);
}

/*
 * Add an option in an help object
 * \param help an initialized help_t object
 * \param short name shortname of the option
 * \param longname long_name of the option
 * \param desc description of the option
 * \param arg argument of the option
 * \param is_arg_opt TRUE is the argument is optionnal
 */
void help_add_option(help_t* help, const char* shortname, const char* longname,
      const char* desc, const char* arg, char is_arg_opt)
{
   if (help == NULL)
      return;
   option_t* opt = lc_malloc0(sizeof(option_t));

   if (shortname != NULL)
      opt->shortname = lc_strdup(shortname);
   if (longname != NULL)
      opt->longname = lc_strdup(longname);
   if (desc != NULL)
      opt->desc = lc_strdup(desc);
   if (arg)
      opt->arg = lc_strdup(arg);
   opt->type = _HELPTYPE_OPT;
   opt->is_arg_opt = is_arg_opt;

   help->size_options += 1;
   help->options = lc_realloc(help->options,
         help->size_options * sizeof(option_t));
   help->options[help->size_options - 1] = opt;
}

/*
 * Add a separator in the option list
 * \param help an initialized help_t object
 * \param name name of the separator
 */
void help_add_separator(help_t* help, const char* name)
{
   if (help == NULL)
      return;
   option_t* opt = lc_malloc0(sizeof(option_t));

   if (name != NULL)
      opt->longname = lc_strdup(name);
   opt->type = _HELPTYPE_SEP;

   help->size_options += 1;
   help->options = lc_realloc(help->options,
         help->size_options * sizeof(option_t));
   help->options[help->size_options - 1] = opt;
}

/*
 * Set the help description section (it must been formatted)
 * \param help an initialized help_t object
 * \param desc the description
 */
void help_set_description(help_t* help, const char* desc)
{
   if (help == NULL || desc == NULL)
      return;
   help->description = lc_strdup(desc);
}

/*
 * Set the help usage section (it must been formatted)
 * \param help an initialized help_t object
 * \param usage the usage
 */
void help_set_usage(help_t* help, const char* usage)
{
   if (help == NULL || usage == NULL)
      return;
   help->usage = lc_strdup(usage);
}

/*
 * Set the email address for bug reporting
 * \param help an initialized help_t object
 * \param addr email address for bug reporting
 */
void help_set_bugs(help_t* help, const char* addr)
{
   if (help == NULL || addr == NULL)
      return;
   help->bugs = lc_strdup(addr);
}

/*
 * Set the copyright
 * \param help an initialized help_t object
 * \param copyright the program copyright
 */
void help_set_copyright(help_t* help, const char* copyright)
{
   if (help == NULL || copyright == NULL)
      return;
   help->copyright = lc_strdup(copyright);
}

/*
 * Set the author
 * \param help an initialized help_t object
 * \param author the author
 */
void help_set_author(help_t* help, const char* author)
{
   if (help == NULL || author == NULL)
      return;
   help->author = lc_strdup(author);
}

/*
 * Set the program name
 * \param help an initialized help_t object
 * \param prog the program name
 */
void help_set_program(help_t* help, const char* prog)
{
   if (help == NULL || prog == NULL)
      return;
   help->program = lc_strdup(prog);
}

/*
 * Set the version
 * \param help an initialized help_t object
 * \param version the version
 */
void help_set_version(help_t* help, const char* version)
{
   if (help == NULL || version == NULL)
      return;
   help->version = lc_strdup(version);
}

/*
 * Set the date
 * \param help an initialized help_t object
 * \param date the date
 */
void help_set_date(help_t* help, const char* date)
{
   if (help == NULL || date == NULL)
      return;
   help->date = lc_strdup(date);
}

/*
 * Set the build number
 * \param help an initialized help_t object
 * \param build the build number
 */
void help_set_build(help_t* help, const char* build)
{
   if (help == NULL || build == NULL)
      return;
   help->build = lc_strdup(build);
}

/*
 * Add an example in the help_t object
 * \param help an initialized help_t object
 * \param cmd command of the example
 * \param desc description of the example
 */
void help_add_example(help_t* help, const char* cmd, const char* desc)
{
   if (help == NULL || cmd == NULL || desc == NULL)
      return;

   help->size_examples += 1;
   help->examples = lc_realloc(help->examples,
         help->size_examples * sizeof(char**));
   help->examples[help->size_examples - 1] = lc_malloc(2 * sizeof(char*));
   help->examples[help->size_examples - 1][_EXAMPLE_CMD] = lc_strdup(cmd);
   help->examples[help->size_examples - 1][_EXAMPLE_DESC] = lc_strdup(desc);
}

/*
 * Display the help
 * \param help an initialized help_t object
 * \param output file where to display the help
 */
void help_print(help_t* help, FILE* output)
{
   if (help == NULL || output == NULL)
      return;
   int i = 0;

   fprintf(output, "\nSynopsis:\n  %s\n", help->usage);

   fprintf(output, "\nDescription:\n%s\n", help->description);

   fprintf(output, "\nOptions:\n");
   for (i = 0; i < help->size_options; i++) {
      option_t* opt = help->options[i];
      if (opt->type == _HELPTYPE_SEP) {
         fprintf(output, "  %s\n", opt->longname);
      } else if (opt->type == _HELPTYPE_OPT) {
         if (opt->shortname != NULL)
            fprintf(output, "    -%s, --%s", opt->shortname, opt->longname);
         else if (opt->longname[0] == '<')
            fprintf(output, "        %s", opt->longname);
         else
            fprintf(output, "        --%s", opt->longname);

         // Add the argument if needed
         if (opt->arg != NULL && opt->is_arg_opt == TRUE)
            fprintf(output, "[=%s]", opt->arg);
         else if (opt->arg != NULL)
            fprintf(output, "=%s", opt->arg);
         fprintf(output, "\n");

         // Add description
         unsigned int j = 0;
         fprintf(output, "            ");
         for (j = 0; j < strlen(opt->desc); j++) {
            fprintf(output, "%c", opt->desc[j]);
            if (opt->desc[j] == '\n')
               fprintf(output, "            ");
         }
         fprintf(output, "\n\n");
      }
   }

   if (help->size_examples > 0) {
      fprintf(output, "\nExamples:\n");
      for (i = 0; i < help->size_examples; i++) {
         fprintf(output, "  %s\n      %s\n\n", help->examples[i][_EXAMPLE_CMD],
               help->examples[i][_EXAMPLE_DESC]);
      }
   }

   if (help->bugs != NULL)
      fprintf(output, "\nReport bugs to <%s>\n", help->bugs);
}

/*
 * DIsplay the version
 * \param help an initialized help_t object
 * \param output file where to display the version
 */
void help_version(help_t* help, FILE* output)
{
   if (help == NULL || output == NULL)
      return;

   if (help->program)
      fprintf(output, "%s", help->program);
   if (help->version)
      fprintf(output, " %s", help->version);
   if (help->build)
      fprintf(output, " - %s", help->build);
   fprintf(output, "\n");

   if (help->copyright != NULL)
      fprintf(output, "\n%s\n", help->copyright);

   if (help->author != NULL)
      fprintf(output, "\nWritten by %s.\n", help->author);
}

/*
 * Delete a help_t structure
 * \param help an initialized help_t object
 */
void help_free(help_t* help)
{
   if (help == NULL)
      return;
   int i = 0;

   if (help->options != NULL) {
      for (i = 0; i < help->size_options; i++) {
         option_t* opt = help->options[i];

         if (opt->shortname != NULL)
            lc_free(opt->shortname);
         if (opt->longname != NULL)
            lc_free(opt->longname);
         if (opt->desc != NULL)
            lc_free(opt->desc);
         if (opt->arg != NULL)
            lc_free(opt->arg);
         lc_free(opt);
      }
      lc_free(help->options);
   }
   if (help->examples != NULL) {
      for (i = 0; i < help->size_examples; i++) {
         lc_free(help->examples[i][_EXAMPLE_CMD]);
         lc_free(help->examples[i][_EXAMPLE_DESC]);
         lc_free(help->examples[i]);
      }
      lc_free(help->examples);
   }
   if (help->usage != NULL)
      lc_free(help->usage);
   if (help->description != NULL)
      lc_free(help->description);
   if (help->program != NULL)
      lc_free(help->program);
   if (help->version != NULL)
      lc_free(help->version);
   if (help->bugs != NULL)
      lc_free(help->bugs);
   if (help->copyright != NULL)
      lc_free(help->copyright);
   if (help->author != NULL)
      lc_free(help->author);
   lc_free(help);
}

