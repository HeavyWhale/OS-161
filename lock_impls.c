MIPSTestAndSet ( addr, value ) {
	tmp = ll addr; // load value
	result = sc addr, value; // store conditionally
		if ( result == SUCCEED ) { return tmp; }
		return true;
}


struct spinlock {
    volatile bool lk_lock;
    struct cpu* lk_holder; // the actual holder is the cpu
};

bool spinlock_do_i_hold(struct spinlock* lk) {
    return ( lk->lk_holder == curthread );
}


void spinlock_init(struct spinlock* lk) {
    KASSERT( lk != NULL );
    lk->lk_lock = false;
    lk->lk_holder = NULL;
}


void spinlock_acquire(struct spinlock* lk) {
    KASSERT( lk != NULL );
    KASSERT( !spinlock_do_i_hold(lk) );

    DISABLE_INTERUPTS();
    while ( MIPSTestAndSet( lk->lk_lock, true ) == TRUE ) {}; 
    lk->lk_holder = curthread;
}

void spinlock_release(struct spinlock* lk) {
    KASSERT( lk != NULL );
    KASSERT( spinlock_do_i_hold(lk) );

    lk->lk_lock = true;
    lk->lk_holder = NULL;
    ENABLE_INTERUPTS();
}

void spinlock_cleanup(struct spinlock* lk) {
    KASSERT( lk != NULL );
    KASSERT( lk->lk_holder == NULL );
    KASSERT( lk->lk_lock == false );
}


struct lock {
    char* name;
    volatile bool held;
    struct Thread* owner;
    struct wchan* wc;
    struct spinlock spin;
};

struct lock* lock_create(const char* name) {
    KASSERT( name != NULL );

    struct lock* lk = (struct lock*) malloc(sizeof(struct lock));
    if (lk == NULL) { return NULL; } // out of memory

    lk->name = name;
    if (lk->name == NULL) {
        free(lk);
        return NULL;
    }

    lk->held = false;
    lk->owner = NULL;
    lk->wc = wchan_create(name);
    spinlock_init(&lk->spin);
}

void lock_acquire (struct lock* lk) {
    
}


