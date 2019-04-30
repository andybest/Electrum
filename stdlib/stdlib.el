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
  (def-ffi-fn* + rt_add :el (:el :el))
  (def-ffi-fn* - rt_sub :el (:el :el))
  (def-ffi-fn* * rt_mul :el (:el :el))
  (def-ffi-fn* / rt_div :el (:el :el))

                                        ; Predicates
  (def-ffi-fn* eq? rt_eq :el (:el :el))

                                        ; Boolean operators
  (def-ffi-fn* and rt_and :el (:el :el))
  (def-ffi-fn* or  rt_or :el (:el :el))
  (def-ffi-fn* not rt_not :el (:el))

  (def-ffi-fn* apply rt_apply :el (:el :el))

  (defmacro defn (name args & body)
    (list 'def name (cons 'lambda (cons args body))))


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

  (defmacro cond (& clauses)
    (let ((rest-clauses clauses)
          (result nil))
      (while (not (nil? rest-clauses))
        (let ((pred (caar rest-clauses))
              (expr (cadar rest-clauses)))
          (set! result (append result (list 'if pred expr)))
          (set! rest-clauses (cdr rest-clauses))))
      (append result 'nil)))

                                        ;(defmacro quasiquote (expr)
                                        ;  (if (not (list? expr))
                                        ;      expr
                                        ;    (if (eq? (car expr) 'unquote)))
  )


