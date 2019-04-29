
(eval-when (:compile :load)
    ; Type predicates
    (def-ffi-fn* integer? rt_is_integer :el (:el))
    (def-ffi-fn* float?   rt_is_float   :el (:el))
    (def-ffi-fn* boolean? rt_is_boolean :el (:el))
    (def-ffi-fn* symbol?  rt_is_symbol :el (:el))
    (def-ffi-fn* keyword? rt_is_keyword :el (:el))
    (def-ffi-fn* string?  rt_is_string :el (:el))

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
)