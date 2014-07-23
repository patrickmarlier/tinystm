

extern void *__cxa_allocate_exception (size_t) __attribute__((weak));
extern void __cxa_throw (void *, void *, void *) __attribute__((weak));
extern void *__cxa_begin_catch (void *) __attribute__((weak));
extern void *__cxa_end_catch (void) __attribute__((weak));
/* TODO check if TM_ABI -> no problem of dependancy of gcc-tm  */
extern void __cxa_tm_cleanup (void *, void *, unsigned int) __attribute__((weak));


void *_ITM_cxa_allocate_exception (size_t size)
{
  void *r = __cxa_allocate_exception (size);
  /*tx->cxa_unthrown = r;*/
  return r;
}

void _ITM_cxa_throw (void *obj, void *tinfo, void *dest)
{
  /*tx->cxa_unthrown = NULL;*/
  __cxa_throw (obj, tinfo, dest);
}

void *_ITM_cxa_begin_catch (void *exc_ptr)
{
  /*tx->cxa_catch_count++;*/
  return __cxa_begin_catch (exc_ptr);
}

void _ITM_cxa_end_catch (void)
{
  /*tx->cxa_catch_count--;*/
  __cxa_end_catch ();
}

/* On rollback */
/*
 * TODO integrate this to completely makes work exception with GCC-TM
void stm_revert_cpp_exceptions (void)
{   
  if (tx->cxa_unthrown || tx->cxa_catch_count) {
    __cxa_tm_cleanup (tx->cxa_unthrown, tx->eh_in_flight,
                      tx->cxa_catch_count);
    tx->cxa_catch_count = 0;
    tx->cxa_unthrown = NULL;
    tx->eh_in_flight = NULL;
  }
  if (tx->eh_in_flight) {
    _Unwind_DeleteException ((_Unwind_Exception *) tx->eh_in_flight);
    tx->eh_in_flight = NULL;
  }
}

in _ITM_commitTransactionEH
tx->eh_in_flight = exc_ptr;
in _ITM_beginTransaction (no nesting)
tx->eh_in_flight = NULL;
*/

