/* Arduino avr-gcc 7.3 libgcov.a has __gcov_dump but not __gcov_exit.
   Coverage constructors emit a destructor that calls __gcov_exit. */
#if defined(RR_GCOV)
void __gcov_dump(void);

void __gcov_exit(void) { __gcov_dump(); }
#endif
