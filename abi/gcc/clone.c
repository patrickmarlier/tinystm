/* This file is inspired by the work of Richard Henderson on the 
 * GNU Transactional Memory Library (libitm).
 */

/* No include needed since the file is included */

/* FIXME implement a read/write lock for tables */
//static gtm_rwlock table_lock;

struct clone_entry
{
  void *orig, *clone;
};

struct clone_table
{
  struct clone_entry *table;
  size_t size;
  struct clone_table *next;
};

static struct clone_table *all_tables;

static void *
find_clone (void *ptr)
{
  struct clone_table *table;
  void *ret = NULL;

//  table_lock.read_lock ();

  for (table = all_tables; table ; table = table->next)
    {
      struct clone_entry *t = table->table;
      size_t lo = 0, hi = table->size, i;

      /* Quick test for whether PTR is present in this table.  */
      if (ptr < t[0].orig || ptr > t[hi - 1].orig)
	continue;

      /* Otherwise binary search.  */
      while (lo < hi)
	{
	  i = (lo + hi) / 2;
	  if (ptr < t[i].orig)
	    hi = i;
	  else if (ptr > t[i].orig)
	    lo = i + 1;
	  else
	    {
	      ret = t[i].clone;
	      goto found;
	    }
	}

      /* Given the quick test above, if we don't find the entry in
	 this table then it doesn't exist.  */
      break;
    }

 found:
//  table_lock.read_unlock ();
  return ret;
}


void * _ITM_CALL_CONVENTION
_ITM_getTMCloneOrIrrevocable (void *ptr)
{
  // if the function (ptr) have a TM version, give the pointer to the TM function 
  // otherwise, set transaction to irrevocable mode
  void *ret = find_clone (ptr);
  if (ret)
    return ret;

  /* TODO Check we are in an active transaction */
  //  if (stm_current_tx() != NULL && stm_is_active(tx))
    /* GCC always use implicit transaction descriptor */
    stm_set_irrevocable(1);

  return ptr;
}

void * _ITM_CALL_CONVENTION
_ITM_getTMCloneSafe (void *ptr)
{
  void *ret = find_clone (ptr);
  if (ret == NULL)
    abort ();
  return ret;
}

static int
clone_entry_compare (const void *a, const void *b)
{
  const struct clone_entry *aa = (const struct clone_entry *)a;
  const struct clone_entry *bb = (const struct clone_entry *)b;

  if (aa->orig < bb->orig)
    return -1;
  else if (aa->orig > bb->orig)
    return 1;
  else
    return 0;
}

void
_ITM_registerTMCloneTable (void *xent, size_t size)
{
  struct clone_entry *ent = (struct clone_entry *)(xent);
  struct clone_table *old, *table;

  table = (struct clone_table *) malloc (sizeof (struct clone_table));
  table->table = ent;
  table->size = size;

  qsort (ent, size, sizeof (struct clone_entry), clone_entry_compare);

  old = all_tables;
  do
    {
      table->next = old;
      /* TODO Change to use AtomicOps wrapper */
      old = __sync_val_compare_and_swap (&all_tables, old, table);
    }
  while (old != table);
}

void
_ITM_deregisterTMCloneTable (void *xent)
{
  struct clone_entry *ent = (struct clone_entry *)(xent);
  struct clone_table **pprev = &all_tables;
  struct clone_table *tab;

//  table_lock.write_lock ();

  for (pprev = &all_tables;
       tab = *pprev, tab->table != ent;
       pprev = &tab->next)
    continue;
  *pprev = tab->next;

//  table_lock.write_unlock ();

  free (tab);
}

