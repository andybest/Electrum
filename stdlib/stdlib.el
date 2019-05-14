(eval-when (:compile :load)
                                        ; Type predicates
  (def-ffi-fn* nil?     rt_is_nil     :el (:el))
  (def-ffi-fn* integer? rt_is_integer :el (:el))
  (def-ffi-fn* float?   rt_is_float   :el (:el))
  (def-ffi-fn* boolean? rt_is_boolean :el (:el))
  (def-ffi-fn* symbol?  rt_is_symbol  :el (:el))
  (def-ffi-fn* keyword? rt_is_keyword :el (:el))
  (def-ffi-fn* string?  rt_is_string  :el (:el))

                                        ; List
  (def-ffi-fn* list? rt_is_pair :el (:el))
  (def-ffi-fn* cons rt_make_pair :el (:el :el))
  (def-ffi-fn* car rt_car :el (:el))
  (def-ffi-fn* cdr rt_cdr :el (:el))

                                        ; Basic arithmetic
  (def-ffi-fn* rt-add rt_add :el (:el :el))
  (def-ffi-fn* rt-sub rt_sub :el (:el :el))
  (def-ffi-fn* rt-mul rt_mul :el (:el :el))
  (def-ffi-fn* rt-div rt_div :el (:el :el))

                                        ; Predicates
  (def-ffi-fn* eq? rt_eq :el (:el :el))

                                        ; Boolean operators
  (def-ffi-fn* rt-and rt_and :el (:el :el))
  (def-ffi-fn* rt-or  rt_or :el (:el :el))
  (def-ffi-fn* not rt_not :el (:el))

  (def-ffi-fn* apply rt_apply :el (:el :el))

  (def-ffi-fn* print* rt_print :el (:el))

                                        ; Exceptions
  (def-ffi-fn* throw el_rt_throw :el (:el))
  (def-ffi-fn* exception el_rt_make_exception :el (:el :el :el))

  (defmacro defn (name args & body)
    (list 'def name (cons 'lambda (cons args body))))

  (defmacro reduce-args (f & args)
    (let* ((red (lambda (f remaining)
                  (if (not (nil? (cdr remaining)))
                      (list f (car remaining) (red f (cdr remaining)))
                    (car remaining)))))
      (let ((result (red f args)))
        (print* result)
        result)))

  (defmacro reduce-args-reverse (f & args)
    (let ((remaining (cdr args))
          (result (car args)))
      (while (not (nil? remaining))
        (set! result (list f result (car remaining)))
        (set! remaining (cdr remaining)))
      (print* result)
      result))

  (defmacro def-rt-multiarg (binding fn)
    (list 'defmacro binding '(& args) (list 'cons ''reduce-args (list 'cons (list 'quote fn) 'args))))

  (defmacro def-rt-multiarg-reverse (binding fn)
    (list 'defmacro binding '(& args) (list 'cons ''reduce-args-reverse (list 'cons (list 'quote fn) 'args))))

  (def-rt-multiarg + rt-add)
  (def-rt-multiarg - rt-sub)
  (def-rt-multiarg * rt-mul)
  (def-rt-multiarg-reverse / rt-div)

  (def-rt-multiarg and rt-and)
  (def-rt-multiarg or rt-or)

  (defn print (& values)
    (let ((is-first #t)
          (rest values))
      (while (not (nil? rest))
        (if is-first
            (set! is-first #f)
          (print* " "))
        (print* (car rest))
        (set! rest (cdr rest))))
    nil)

  (defn append (l1 l2)
    (if (nil? l1)
        l2
      (cons (car l1) (append (cdr l1) l2))))

  (defn caar   (x) (car (car x)))
  (defn cadr   (x) (car (cdr x)))
  (defn cdar   (x) (cdr (car x)))
  (defn cddr   (x) (cdr (cdr x)))
  (defn caaar  (x) (car (car (car x))))
  (defn caadr  (x) (car (car (cdr x))))
  (defn cadar  (x) (car (cdr (car x))))
  (defn caddr  (x) (car (cdr (cdr x))))
  (defn cdaar  (x) (cdr (car (car x))))
  (defn cdadr  (x) (cdr (car (cdr x))))
  (defn cddar  (x) (cdr (cdr (car x))))
  (defn cdddr  (x) (cdr (cdr (cdr x))))
  (defn caaaar (x) (car (car (car (car x)))))
  (defn caaadr (x) (car (car (car (cdr x)))))
  (defn caadar (x) (car (car (cdr (car x)))))
  (defn caaddr (x) (car (car (cdr (cdr x)))))
  (defn cadaar (x) (car (cdr (car (car x)))))
  (defn cadadr (x) (car (cdr (car (cdr x)))))
  (defn caddar (x) (car (cdr (cdr (car x)))))
  (defn cadddr (x) (car (cdr (cdr (cdr x)))))
  (defn cdaaar (x) (cdr (car (car (car x)))))
  (defn cdaadr (x) (cdr (car (car (cdr x)))))
  (defn cdadar (x) (cdr (car (cdr (car x)))))
  (defn cdaddr (x) (cdr (car (cdr (cdr x)))))
  (defn cddaar (x) (cdr (cdr (car (car x)))))
  (defn cddadr (x) (cdr (cdr (car (cdr x)))))
  (defn cdddar (x) (cdr (cdr (cdr (car x)))))
  (defn cddddr (x) (cdr (cdr (cdr (cdr x)))))

  (defn last (l)
    (let* ((f (lambda (l)
                (if (nil? (cdr l))
                          (car l)
                  (f (cdr l))))))
      (f l)))

  (defmacro when (condition & body)
    (list 'if condition (append (list 'do ) body) nil))

  (defmacro unless (condition & body)
    (list 'if condition nil (append (list 'do) body)))

  (defmacro cond (& clauses)
    (let* ((process-clauses (lambda (clauses)
                             (if (nil? clauses)
                                 nil
                               (if (eq? (caar clauses) :else)
                                   (cadar clauses)
                                 (list 'if (caar clauses)
                                       (cadar clauses)
                                       (process-clauses (cdr clauses))))))))
      (process-clauses clauses)))

  (defn assert-one (x)
    (if
        (cond
         ((nil? x) #t)
         ((not (list? x)) #t)
         ((nil? (cdr x)) #t)
         ((not (nil? (cddr x))) #t)
         (:else #f))
        (throw 'invalid-argument "Expected one arg" nil)
      nil))


  ;(defmacro quasiquote (expr)
  ;  (let ((assert-one-arg (lambda (l)
  ;                          (unless (cond
  ;                                   ((nil? x) #t)
  ;                                   ((nil? (cdr x) #t))
  ;                                   ((not (nil? (cddr x))))
  ;                                   (:else #f))
  ;                            (throw 'invalid-argument "Expected one arg" nil)))))
  ;    (cond
  ;     ((not (list? expr)) (list 'quote expr))
  ;     ((eq? (car expr) 'unquote) (do
  ;                                    (assert-one-arg expr)
  ;                                    (cadr expr))))))

  )


