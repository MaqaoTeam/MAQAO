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
#include <inttypes.h>

#include "libmasm.h"

///////////////////////////////////////////////////////////////////////////////
//                                  label                                    //
///////////////////////////////////////////////////////////////////////////////
/*
 * Create a new label
 *	\param name name of the label
 * \param add address where the label is in the code
 * \param target_type type of the object pointed by the label
 * \param t object pointed by the label
 * \return an initialized label
 */
label_t* label_new(char* name, int64_t add, target_type_t target_type, void* t)
{
   label_t* l = lc_malloc0(sizeof(label_t));
   l->name = lc_strdup(name);
   l->address = add;
   l->target_type = target_type;
   l->type = LBL_GENERIC;
   l->target = t;

   return (l);
}

/*
 * Copies a label
 * \param label Label to copy
 * \return Copy of \c label or NULL if \c label is NULL. Its target and section will be identical to the origin
 * */
label_t* label_copy(label_t* label)
{
   if (!label)
      return NULL;
   label_t* l = label_new(label->name, label->address, label->target_type,
         label->target);
   l->type = label->type;
   l->scn = label->scn;
   return l;
}

/*
 * Retrieves the address of a label
 * \param lbl The label
 * \return The address associated to the label
 * */
int64_t label_get_addr(label_t* lbl)
{
   return ((lbl != NULL) ? lbl->address : ADDRESS_ERROR );
}

/*
 * Retrieves the name of a label
 * \param lbl The label
 * \return The name of the label
 * */
char* label_get_name(label_t* lbl)
{
   return ((lbl != NULL) ? lbl->name : PTR_ERROR);
}

/*
 * Retrieves the type of a label
 * \param lbl The label
 * \return The type of the label or LBL_ERROR if \c lbl is NULL
 * */
label_type_t label_get_type(label_t* lbl)
{
   return ((lbl != NULL) ? lbl->type : LBL_ERROR);
}

/*
 * Checks if a label can be used to identify a function
 * \param lbl The label
 * \return TRUE if the label is not NULL and has a type that can be associated to a function, FALSE otherwise
 * */
int label_is_type_function(label_t* lbl)
{
   return (lbl && lbl->type < LBL_NOFUNCTION) ? TRUE : FALSE;
}

/*
 * Sets the type of a label
 * \param lbl The label
 * \param type The type of the label
 * */
void label_set_type(label_t* lbl, label_type_t type)
{
   if (lbl != NULL)
      lbl->type = type;
}

/*
 * Retrieves the type of the target of a label
 * \param lbl The label
 * \return The type of target of the label
 * */
target_type_t label_get_target_type(label_t* lbl)
{
   return ((lbl != NULL) ? lbl->target_type : TARGET_UNDEF);
}

/*
 * Retrieves the object associated to a label
 * \param lbl The label
 * \return The object pointed to by the label
 * */
void* label_get_target(label_t* lbl)
{
   return ((lbl != NULL) ? lbl->target : PTR_ERROR);
}

/*
 * Retrieves the section a label belongs to
 * \param lbl The label
 * \return The section from the binary file where the label is defined, or NULL if lbl is NULL
 * */
binscn_t* label_get_scn(label_t* lbl)
{
   return ((lbl) ? lbl->scn : PTR_ERROR);
}

/*
 * Associates an instruction to a label
 * \param lbl The label
 * \param insn The instruction to associate
 * */
void label_set_target_to_insn(label_t* lbl, insn_t* insn)
{
   if (lbl != NULL) {
      DBGMSGLVL(1, "Linking label %s (%p) to instruction %p\n", lbl->name, lbl,
            insn);
      lbl->target = insn;
      lbl->target_type = TARGET_INSN;
   }
}

/*
 * Associates a data entry to a label
 * \param lbl The label
 * \param data The data entry to associate
 * */
void label_set_target_to_data(label_t* lbl, data_t* data)
{
   if (lbl != NULL) {
      DBGMSGLVL(1, "Linking label %s (%p) to data entry %p\n", lbl->name, lbl,
            data);
      lbl->target = data;
      lbl->target_type = TARGET_DATA;
//      lbl->type = LBL_VARIABLE;
   }
}

/*
 * Sets the address of a label
 * \param lbl The label
 * \param address The address to be associated to the label
 * */
void label_set_addr(label_t* lbl, int64_t address)
{
   if (lbl != NULL)
      lbl->address = address;
}

/*
 * Sets the section a label belongs to
 * \param lbl The label
 * \param scn The section from the binary file where the label is defined
 * */
void label_set_scn(label_t* lbl, binscn_t* scn)
{
   if (lbl)
      lbl->scn = scn;
}

/*
 * Updates the address of a label depending on the address of its target
 * \param lbl The label
 * */
void label_upd_addr(label_t* lbl)
{
   if (!lbl || !lbl->target)
      return;  //Exiting if label is NULL or has no target
   switch (lbl->target_type) {
   case TARGET_INSN:
      lbl->address = insn_get_addr(lbl->target);
      break;
   case TARGET_DATA:
      lbl->address = data_get_addr(lbl->target);
      break;
   case TARGET_BSCN:
      lbl->address = binscn_get_addr(lbl->target);
      break;
   }  //Otherwise the target type is unknown and the address can't be updated
}

/**
 * Compares two labels based on their addresses.
 * \param a First label
 * \param b Second label
 * \return 1 if the address of \c a is higher than the address of \c b and -1 otherwise
 *  If both have the same address, their types will be used to try breaking the tie: if a has a larger value for type than b, 1 will
 *  be returned, -1 otherwise. If both are equal, the result of the comparison between the names will be returned
 * */
static int label_cmp(label_t* a, label_t* b)
{

   if ((int64_t) label_get_addr(a) - label_get_addr(b) > 0)
      return (1);
   else if (label_get_addr(a) == label_get_addr(b)) {
      if ((int) ((int) label_get_type(a) - (int) label_get_type(b)) > 0)
         return 1;
      else if ((int) ((int) label_get_type(a) - (int) label_get_type(b)) < 0)
         return -1;
      else {
         if ((label_get_name(a) != NULL) && (label_get_name(b) != NULL))
            return (strcmp(label_get_name(a), label_get_name(b)));
         return (0);
      }
   } else
      return (-1);

}

/*
 * Compares two labels based on their addresses (for use with qsort)
 * \param a First label
 * \param b Second label
 * \return The result of the comparison between a and b by \ref label_cmp
 * */
int label_cmp_qsort(const void *a, const void *b)
{
   label_t *A = *((void **) a);
   label_t *B = *((void **) b);

   return label_cmp(A, B);
}

/*
 * Compares the address of a label structure with a given address. This function is intended to be used by bsearch
 * \param addr Pointer to the value of the address to compare
 * \param label Pointer to the label structure
 * \return -1 if address of label is less than address, 0 if both are equal, 1 if address of label is higher than the address
 * */
int label_cmpaddr_forbsearch(const void* address, const void* label)
{
   if (!address || !label)
      return (address != label);
   int64_t* addr = (int64_t*) address;
   label_t** lab = (label_t**) label;
   if ((*lab)->address > *addr)
      return -1;
   else if ((*lab)->address < *addr)
      return 1;
   return 0;
}

/*
 * Add a label into an asmfile. Labels are ordered by address (in case of labels with
 * the same address, the last added label will be the latest in the list)
 * \param lab an existing label
 * \param asmf an existing asmfile
 */
void asmfile_add_label(asmfile_t* asmf, label_t* lab)
{
   if (lab == NULL || asmf == NULL)
      return;

   hashtable_insert(asmf->label_table, (void*) lab->name, (void*) lab);
   label_t* lab_tmp = (label_t*) queue_peek_tail(asmf->label_list);

   if (lab_tmp == NULL) {
      queue_add_head(asmf->label_list, (void*) lab);
   } else if (label_cmp(lab, lab_tmp) >= 0) //The label to insert has a higher or equal address than the last one in the list
         {
      queue_add_tail(asmf->label_list, (void*) lab); //Adding the label after the last label
      DBGMSG("Label %s (%#"PRIx64") was inserted after label %s (%#"PRIx64")\n",
            lab->name, lab->address, lab_tmp->name, lab_tmp->address);
   } else {
      label_t* lab_tmp3 = (label_t*) queue_peek_head(asmf->label_list);
      if (label_cmp(lab, lab_tmp3) < 0) //The label to insert has a lower address than the first one in the list
            {
         queue_add_head(asmf->label_list, (void*) lab); //Adding the label before the first label
         DBGMSG(
               "Label %s (%#"PRIx64") was inserted before label %s (%#"PRIx64")\n",
               lab->name, lab->address, lab_tmp3->name, lab_tmp3->address);
      } else {
         //At this point we know that the label to insert has a higher or equal address than
         //the first label and a strictly lower address than the last label
         FOREACH_INQUEUE(asmf->label_list, it0)
         {
            label_t* lab_tmp2 = GET_DATA_T(label_t*, it0);
            if (label_cmp(lab, lab_tmp2) < 0) //We found the first label with a higher address than the label to insert
                  {
               queue_insertbefore(asmf->label_list, it0, lab);
               DBGMSG(
                     "Label %s (%#"PRIx64") was inserted before label %s (%#"PRIx64")\n",
                     lab->name, lab->address, lab_tmp2->name, lab_tmp2->address);
               break;
            }
         }
      }
   }

   //Now the complicated part: we also have to add this label to the array of function of variable labels
   label_t **labels = NULL;
   uint32_t n_labels;
   if (label_get_type(lab) < LBL_NOFUNCTION) {
      //Label is eligible to be added to the array of function labels
      labels = asmf->fctlabels;
      n_labels = asmf->n_fctlabels;
   } else if (label_get_type(lab) < LBL_NOVARIABLE) {
      // Label can be added to the array of variable labels: same code as above
      labels = asmf->varlabels;
      n_labels = asmf->n_varlabels;
   }
   if (labels) {
      unsigned int i = 0;
      // Finds the first label
      while ((i < n_labels) && (label_cmp(lab, labels[i]) > 0))
         i++;
      if (i == n_labels) { // All labels are smaller than the one added: we add it to the end
         labels = lc_realloc(labels, sizeof(label_t*) * (n_labels + 1));
         labels[n_labels] = lab;
         n_labels++;
      } else if (label_cmp(lab, labels[i]) < 0) { //Tough part: we will have to shift all the labels following the one we insert
         unsigned int j = i;
         labels = lc_realloc(labels, sizeof(label_t*) * (n_labels + 1));
         // Shifts all labels following this one to the next cell
         j = n_labels;
         while (j > i) {
            labels[j] = labels[j - 1];
            j--;
         }
         //Adds the label to the array
         labels[i] = lab;
         n_labels++;
      } //Else the label added is equal to one already existing, so we don't add it to this array
   }
   //Restoring values to the asmfile
   /**\todo TODO (2014-12-01) Find some better way to do this to avoid the double test (we would create a static function taking as parameters
    * a pointer to the array and a pointer to its size)*/
   if (label_get_type(lab) < LBL_NOFUNCTION) {
      asmf->fctlabels = labels;
      asmf->n_fctlabels = n_labels;
   } else if (label_get_type(lab) < LBL_NOVARIABLE) {
      asmf->varlabels = labels;
      asmf->n_varlabels = n_labels;
   }

}

/*
 * Add a label into an asmfile. Labels are not ordered by address
 * \param lab an existing label
 * \param asmf an existing asmfile
 */
void asmfile_add_label_unsorted(asmfile_t* asmf, label_t* lab)
{
   if (lab == NULL || asmf == NULL)
      return;

   hashtable_insert(asmf->label_table, (void*) lab->name, (void*) lab);

   queue_add_tail(asmf->label_list, (void*) lab); //Adding the label after the last label
   DBGMSG("Label %s (%#"PRIx64") was inserted\n", lab->name, lab->address);
}

/**
 * Sorts the list of labels.
 * \param asmf The asmfile. Must not be NULL
 * */
static void asmfile_sortlabel_list(asmfile_t* asmf)
{
   assert(asmf);
   queue_sort(asmf->label_list, label_cmp_qsort);
}

/*
 * Reorders the queue label_list only.
 * \note This function should only be needed when a label search based on address must be performed before asmfile_upd_labels has been invoked
 * \param asmf An existing asmfile
 * */
void asmfile_sort_labels(asmfile_t* asmf)
{
   if (asmf)
      asmfile_sortlabel_list(asmf);
}

/*
 * Updates the labels in an asmfile: reorders the queue label_list and builds the fctlabels array containing only
 * labels eligible to be associated with instructions.
 * \param asmf An existing asmfile
 * */
void asmfile_upd_labels(asmfile_t* asmf)
{

   if (asmf == NULL)
      return;
   // Sorting the labels in the list
   asmfile_sortlabel_list(asmf);

   // Building the array of labels eligible to be associated to instructions
   if (queue_length(asmf->label_list) > 0) {
      unsigned int n_fctlabels = 0, n_varlabels = 0;
      list_t* iter = queue_iterator(asmf->label_list);
      // To reduce the number of allocations, we allocate the maximum size this array could take
      asmf->fctlabels = lc_malloc0(
            sizeof(label_t*) * queue_length(asmf->label_list));
      asmf->varlabels = lc_malloc0(
            sizeof(label_t*) * queue_length(asmf->label_list));
      while (iter != NULL) {
         label_t* lbl = GET_DATA_T(label_t*, iter);
         if (label_get_type(lbl) < LBL_NOFUNCTION) { //This label is eligible
            // Adding it to the array of eligible labels
            asmf->fctlabels[n_fctlabels++] = lbl;
            DBGMSG(
                  "File contains function label %s at address %#"PRIx64" with type %d\n",
                  label_get_name(lbl), label_get_addr(lbl), label_get_type(lbl));
            // Skipping other labels with the same address. There can be only one
            while ((iter != NULL)
                  && (label_get_addr(GET_DATA_T(label_t*, iter))
                        == label_get_addr(lbl)))
               iter = iter->next;
            continue; //We don't want to advance twice in the labels
         } else if (label_get_type(lbl) < LBL_NOVARIABLE) { //This label can be associated to data entries
            // Adding it to the array of eligible labels for data entries
            asmf->varlabels[n_varlabels++] = lbl;
            DBGMSG(
                  "File contains variable label %s at address %#"PRIx64" with type %d\n",
                  label_get_name(lbl), label_get_addr(lbl), label_get_type(lbl));
            // Skipping other labels with the same address. There can be only one
            /**\note (2014-12-02) In theory, there should be no need to skip other labels, as we built those labels
             * to avoid having more than one of this type at a given address for a given section. But there are labels with address 0,
             * and we will probably meet someday the case of sections with overlapping addresses...*/
            while ((iter != NULL)
                  && (label_get_addr(GET_DATA_T(label_t*, iter))
                        == label_get_addr(lbl)))
               iter = iter->next;
            continue; //We don't want to advance twice in the labels
         }
         iter = iter->next;
      }
      if ((n_fctlabels > 0)
            && (n_fctlabels < (unsigned int) queue_length(asmf->label_list))) {
         // There are eligible labels, but less than the total number of labels: we need to crop the unnecessary cells in the array
         asmf->fctlabels = lc_realloc(asmf->fctlabels,
               sizeof(label_t*) * n_fctlabels);
      } else if (n_fctlabels == 0) {
         // Tough luck: we found no eligible labels at all
         lc_free(asmf->fctlabels);
         asmf->fctlabels = NULL;
      }
      if ((n_varlabels > 0)
            && (n_varlabels < (unsigned int) queue_length(asmf->label_list))) {
         // There are eligible labels, but less than the total number of labels: we need to crop the unnecessary cells in the array
         asmf->varlabels = lc_realloc(asmf->varlabels,
               sizeof(label_t*) * n_varlabels);
      } else if (n_varlabels == 0) {
         // Tough luck: we found no eligible labels at all
         lc_free(asmf->varlabels);
         asmf->varlabels = NULL;
      }
      // Updating the size of the array in the asmfile
      asmf->n_fctlabels = n_fctlabels;
      asmf->n_varlabels = n_varlabels;
   }
}

/*
 * Free an existing label
 * \param p a pointer on a label
 */
void label_free(void* p)
{
   label_t* l = (label_t*) p;

   if (p == NULL)
      return;

   str_free(l->name);
   lc_free(l);
}

