# Kryon Lisp Language Specification

Complete specification for Kryon Lisp - a modern Lisp dialect optimized for UI development, reactive programming, and seamless integration with the Kryon ecosystem.

> **🔗 Related Documentation**: 
> - [KRY_LANGUAGE_SPECIFICATION.md](KRY_LANGUAGE_SPECIFICATION.md) - Main Kryon UI language
> - [KRYON_SCRIPT_SYSTEM.md](../runtime/KRYON_SCRIPT_SYSTEM.md) - Script integration system

## Table of Contents
- [Language Overview](#language-overview)
- [Syntax & Grammar](#syntax--grammar)
- [Data Types](#data-types)
- [Functions & Macros](#functions--macros)
- [UI Integration](#ui-integration)
- [Reactive Programming](#reactive-programming)
- [Standard Library](#standard-library)
- [Interoperability](#interoperability)
- [Performance & Optimization](#performance--optimization)
- [Advanced Features](#advanced-features)

---

## Language Overview

Kryon Lisp is a modern, functional Lisp dialect designed specifically for UI development and reactive programming. It combines the power and expressiveness of Lisp with modern language features and seamless integration with the Kryon UI system.

### Design Principles

1. **UI-First**: Built-in support for UI elements and reactive updates
2. **Modern Lisp**: Contemporary syntax with optional infix notation
3. **Performance**: Compiled to efficient bytecode
4. **Integration**: Seamless interop with KRY, JavaScript, Lua, and native code
5. **Developer Experience**: Rich tooling, debugging, and error reporting

### Core Features

```lisp
;; Modern syntax with traditional power
(defui my-app []
  (container {:flex-direction "column" :gap 16}
    (text {:size 24 :weight 700} "Welcome to Kryon Lisp")
    (button {:on-click handle-click} "Click me!")
    (input {:value @username :on-change update-username})))

;; Reactive state management
(def-atom username "")
(def-atom counter 0)

;; Event handling
(defn handle-click []
  (swap! counter inc)
  (log "Counter is now:" @counter))
```

---

## Syntax & Grammar

### 1.1 Basic Syntax

Kryon Lisp follows traditional Lisp syntax with modern enhancements:

```lisp
;; Comments
; Single line comment
;; Multi-line comment
#| Block comment
   can span multiple lines |#

;; Basic forms
(function-name arg1 arg2 arg3)
(+ 1 2 3)  ; => 6
(* 2 (+ 3 4))  ; => 14

;; Symbols and keywords
my-symbol
:my-keyword
::namespaced-keyword

;; Literals
42          ; Integer
3.14        ; Float
"string"    ; String
true false  ; Booleans
nil         ; Null value
```

### 1.2 Modern Syntax Extensions

```lisp
;; Optional infix notation for common operations
(= x (+ y 5))     ; Traditional
(= x (y + 5))     ; Infix (optional)

;; Array/object access
(get data :key)         ; Traditional
(data.:key)             ; Modern accessor
(data.key)              ; Simplified

;; Pipeline operator
(->> data
     (filter even?)
     (map double)
     (take 10))

;; Threading macro
(-> user
    :profile
    :settings
    :theme)
```

### 1.3 Reader Macros

```lisp
;; Vector literal
[1 2 3 4]  ; => (vector 1 2 3 4)

;; Map literal  
{:key "value" :count 42}  ; => (hash-map :key "value" :count 42)

;; Set literal
#{1 2 3}  ; => (hash-set 1 2 3)

;; Atom deref
@my-atom  ; => (deref my-atom)

;; Quote
'symbol   ; => (quote symbol)

;; Quasiquote and unquote
`(list ~x ~@args)  ; => (quasiquote (list (unquote x) (unquote-splicing args)))

;; Anonymous function
#(+ %1 %2)  ; => (fn [%1 %2] (+ %1 %2))
```

---

## Data Types

### 2.1 Primitive Types

```lisp
;; Numbers
42           ; Integer
-17          ; Negative integer
3.14159      ; Float
1/3          ; Rational
2+3i         ; Complex (optional)

;; Characters and Strings
\c           ; Character
"hello"      ; String
"multi-line
string"      ; Multi-line string
r"raw\string"; Raw string (no escaping)

;; Booleans and Nil
true
false
nil

;; Keywords
:keyword
::namespaced/keyword
```

### 2.2 Collection Types

```lisp
;; Lists (linked lists)
'(1 2 3 4)
(list 1 2 3 4)

;; Vectors (array-like)
[1 2 3 4]
(vector 1 2 3 4)

;; Maps (hash tables)
{:name "John" :age 30}
(hash-map :name "John" :age 30)

;; Sets
#{1 2 3 4}
(hash-set 1 2 3 4)

;; Sequences (lazy)
(range 10)          ; => (0 1 2 3 4 5 6 7 8 9)
(repeat 5 :x)       ; => (:x :x :x :x :x)
(cycle [1 2])       ; => (1 2 1 2 1 2 ...)
```

### 2.3 UI-Specific Types

```lisp
;; Elements
(defui my-component [] 
  (div {:class "container"}))

;; Atoms (reactive state)
(def-atom counter 0)
(def-atom user {:name "Alice" :role "admin"})

;; Styles
(def-style button-style
  {:background-color "#007bff"
   :color "white"
   :padding [12 20]
   :border-radius 6})

;; Events
(def-event click-handler [event]
  (log "Clicked!" event))
```

---

## Functions & Macros

### 3.1 Function Definition

```lisp
;; Basic function
(defn greet [name]
  (str "Hello, " name "!"))

;; Multi-arity function
(defn add
  ([x] x)
  ([x y] (+ x y))
  ([x y & more] (apply + x y more)))

;; Anonymous functions
(fn [x] (* x x))          ; Traditional
#(* % %)                  ; Short form
#(+ %1 %2)               ; Multiple args

;; Higher-order functions
(defn apply-twice [f x]
  (f (f x)))

(apply-twice #(* % 2) 5)  ; => 20
```

### 3.2 Destructuring

```lisp
;; Vector destructuring
(let [[x y z] [1 2 3]]
  (+ x y z))  ; => 6

;; Map destructuring
(let [{:keys [name age]} {:name "Alice" :age 30}]
  (str name " is " age " years old"))

;; Function parameter destructuring
(defn process-user [{:keys [name email role]}]
  (when (= role "admin")
    (send-notification email (str "Hello " name))))

;; Nested destructuring
(let [{:keys [user]} {:user {:name "Bob" :settings {:theme "dark"}}}
      {:keys [name settings]} user
      {:keys [theme]} settings]
  theme)  ; => "dark"
```

### 3.3 Macros

```lisp
;; Basic macro
(defmacro when-not [condition & body]
  `(if (not ~condition)
     (do ~@body)))

;; UI-specific macros
(defmacro defui [name params & body]
  `(defn ~name ~params
     (ui-element ~@body)))

;; Conditional compilation
(defmacro when-platform [platform & body]
  (when (= *platform* platform)
    `(do ~@body)))

;; Usage
(when-platform :mobile
  (enable-touch-gestures!)
  (setup-haptic-feedback!))
```

### 3.4 Control Flow

```lisp
;; Conditionals
(if condition
  then-value
  else-value)

(when condition
  (do-something)
  (do-something-else))

(cond
  (< x 0) "negative"
  (= x 0) "zero"
  (> x 0) "positive")

(case operation
  :add (+ x y)
  :sub (- x y)
  :mul (* x y)
  :div (/ x y)
  (throw (str "Unknown operation: " operation)))

;; Loops
(loop [i 0 acc []]
  (if (< i 10)
    (recur (inc i) (conj acc i))
    acc))

;; For comprehensions
(for [x (range 5)
      y (range 5)
      :when (even? (+ x y))]
  [x y])
```

---

## UI Integration

### 4.1 UI Elements

```lisp
;; Basic elements
(defui simple-view []
  (div {:class "container"}
    (h1 {} "Welcome")
    (p {} "This is Kryon Lisp!")
    (button {:on-click handle-click} "Click me")))

;; Component composition
(defui user-card [{:keys [name email avatar]}]
  (div {:class "user-card"}
    (img {:src avatar :alt name})
    (div {:class "user-info"}
      (h3 {} name)
      (p {} email))))

(defui user-list [{:keys [users]}]
  (div {:class "user-list"}
    (for [user users]
      (user-card user))))
```

### 4.2 Props and State

```lisp
;; Component with props
(defui counter [{:keys [initial-value label]}]
  (let [count (atom (or initial-value 0))]
    (div {:class "counter"}
      (label {} (or label "Count:"))
      (span {:class "count"} @count)
      (button {:on-click #(swap! count inc)} "+")
      (button {:on-click #(swap! count dec)} "-"))))

;; Using components
(defui app []
  (div {}
    (counter {:initial-value 5 :label "Score:"})
    (counter {:initial-value 0 :label "Lives:"})))
```

### 4.3 Event Handling

```lisp
;; Event handlers
(defn handle-click [event]
  (log "Button clicked!" event)
  (.preventDefault event))

(defn handle-input-change [event]
  (let [value (.-value (.-target event))]
    (reset! input-value value)))

;; Inline handlers
(defui interactive-form []
  (let [username (atom "")
        email (atom "")]
    (form {:on-submit #(submit-form @username @email)}
      (input {:type "text"
              :value @username
              :on-change #(reset! username (.-value (.-target %)))})
      (input {:type "email"
              :value @email
              :on-change #(reset! email (.-value (.-target %)))})
      (button {:type "submit"} "Submit"))))
```

### 4.4 Styling

```lisp
;; Inline styles
(div {:style {:background-color "#f0f0f0"
              :padding "20px"
              :border-radius "8px"}}
  "Styled content")

;; Style definitions
(def-style card-style
  {:background "white"
   :box-shadow "0 2px 4px rgba(0,0,0,0.1)"
   :border-radius "8px"
   :padding "16px"})

(def-style button-primary
  {:background-color "#007bff"
   :color "white"
   :border "none"
   :padding [12 24]
   :border-radius "6px"
   :cursor "pointer"
   :&:hover {:background-color "#0056b3"}})

;; Using styles
(div {:style card-style}
  (button {:style button-primary :on-click handle-click}
    "Primary Button"))
```

---

## Reactive Programming

### 5.1 Atoms and State

```lisp
;; Creating atoms
(def counter (atom 0))
(def user (atom {:name "Alice" :role "user"}))
(def todos (atom []))

;; Reading atoms
@counter              ; => 0
(deref counter)       ; => 0

;; Updating atoms
(reset! counter 5)
(swap! counter inc)   ; counter is now 6
(swap! counter + 10)  ; counter is now 16

;; Complex updates
(swap! user assoc :role "admin")
(swap! todos conj {:id 1 :text "Learn Lisp" :done false})
```

### 5.2 Watchers and Reactions

```lisp
;; Watching atoms
(add-watch counter :counter-watcher
  (fn [key atom old-state new-state]
    (log "Counter changed from" old-state "to" new-state)))

;; Reactive computations
(def-reaction doubled-counter
  (* 2 @counter))

(def-reaction user-display-name
  (let [{:keys [name role]} @user]
    (str name " (" role ")")))

;; UI automatically updates when atoms change
(defui reactive-display []
  (div {}
    (p {} "Counter: " @counter)
    (p {} "Doubled: " @doubled-counter)
    (p {} "User: " @user-display-name)))
```

### 5.3 Effects and Side Effects

```lisp
;; Effects that run when state changes
(def-effect save-counter-to-storage
  [counter]
  (local-storage/set-item "counter" @counter))

(def-effect load-user-data
  [user-id]
  (when @user-id
    (go
      (let [user-data (<! (fetch-user @user-id))]
        (reset! user user-data)))))

;; Cleanup effects
(def-effect setup-websocket
  [connected?]
  (when @connected?
    (let [socket (websocket/connect "ws://localhost:8080")]
      (websocket/on-message socket handle-message)
      ;; Return cleanup function
      #(websocket/close socket))))
```

---

## Standard Library

### 6.1 Core Functions

```lisp
;; Arithmetic
(+ 1 2 3)           ; => 6
(- 10 3)            ; => 7
(* 2 3 4)           ; => 24
(/ 12 3)            ; => 4
(mod 10 3)          ; => 1
(inc 5)             ; => 6
(dec 5)             ; => 4

;; Comparison
(= 1 1)             ; => true
(not= 1 2)          ; => true
(< 1 2)             ; => true
(> 2 1)             ; => true
(<= 1 1)            ; => true
(>= 2 1)            ; => true

;; Logic
(and true false)    ; => false
(or false true)     ; => true
(not true)          ; => false
```

### 6.2 Collection Functions

```lisp
;; Creation
(list 1 2 3)        ; => (1 2 3)
(vector 1 2 3)      ; => [1 2 3]
(hash-map :a 1 :b 2); => {:a 1 :b 2}
(hash-set 1 2 3)    ; => #{1 2 3}

;; Access
(first [1 2 3])     ; => 1
(rest [1 2 3])      ; => (2 3)
(last [1 2 3])      ; => 3
(nth [1 2 3] 1)     ; => 2
(get {:a 1} :a)     ; => 1

;; Transformation
(map inc [1 2 3])           ; => (2 3 4)
(filter even? [1 2 3 4])   ; => (2 4)
(reduce + [1 2 3 4])       ; => 10
(take 3 (range 10))        ; => (0 1 2)
(drop 3 [1 2 3 4 5])       ; => (4 5)

;; Modification
(conj [1 2] 3)      ; => [1 2 3]
(assoc {:a 1} :b 2) ; => {:a 1 :b 2}
(dissoc {:a 1 :b 2} :a) ; => {:b 2}
(update {:count 5} :count inc) ; => {:count 6}
```

### 6.3 String Functions

```lisp
;; String operations
(str "Hello" " " "World")     ; => "Hello World"
(count "hello")               ; => 5
(subs "hello" 1 3)           ; => "el"
(clojure.string/upper-case "hello") ; => "HELLO"
(clojure.string/lower-case "HELLO") ; => "hello"
(clojure.string/split "a,b,c" #",")  ; => ["a" "b" "c"]
(clojure.string/join "," [1 2 3])    ; => "1,2,3"

;; String formatting
(format "Hello %s, you are %d years old" "Alice" 30)
; => "Hello Alice, you are 30 years old"
```

### 6.4 I/O Functions

```lisp
;; Console output
(print "Hello")        ; Print without newline
(println "World")      ; Print with newline
(prn {:a 1 :b 2})     ; Print for debugging

;; Logging
(log "Debug message")
(warn "Warning message")
(error "Error message")

;; File I/O (where supported)
(slurp "file.txt")           ; Read entire file
(spit "file.txt" "content")  ; Write to file
```

---

## Interoperability

### 7.1 JavaScript Interop

```lisp
;; Calling JavaScript functions
(js/alert "Hello from Lisp!")
(js/console.log "Debug message")
(js/Math.random)

;; Creating JavaScript objects
(js-obj "name" "Alice" "age" 30)
; => #js {:name "Alice" :age 30}

;; Accessing JavaScript properties
(.-length "hello")      ; => 5
(.-name js/document)    ; => "document"

;; Calling methods
(.getElementById js/document "my-id")
(.push array "new-item")

;; Converting between Lisp and JS
(clj->js {:a 1 :b 2})   ; Convert to JavaScript object
(js->clj js-object)     ; Convert from JavaScript object
```

### 7.2 KRY Integration

```lisp
;; Embedding Lisp in KRY files
; In a .kry file:
; Container {
;     @script "lisp" {
;         (defn handle-click []
;           (swap! counter inc))
;         
;         (defui dynamic-content []
;           (if (> @counter 5)
;             (text {} "High count!")
;             (text {} "Low count")))
;     }
; }

;; Calling KRY functions from Lisp
(kryon/set-variable "username" "Alice")
(kryon/get-variable "counter")
(kryon/emit-event "user-login" {:user-id 123})

;; Accessing KRY elements
(kryon/get-element-by-id "my-button")
(kryon/set-property "my-text" "content" "New text")
```

### 7.3 Native Integration

```lisp
;; Platform-specific code
(when-platform :ios
  (native/vibrate :light)
  (native/show-alert "iOS Alert"))

(when-platform :android
  (native/vibrate 100)
  (native/show-toast "Android Toast"))

;; Native API access
(native/camera-capture 
  {:quality 0.8}
  (fn [photo]
    (reset! current-photo photo)))

(native/location-watch
  {:accuracy :high}
  (fn [location]
    (reset! user-location location)))
```

---

## Performance & Optimization

### 8.1 Compilation

```lisp
;; Compile-time evaluation
(def pi (Math/PI))           ; Evaluated at compile time
(def version "1.0.0")        ; Constant folding

;; Macro expansion happens at compile time
(defmacro debug-log [msg]
  (when *debug*
    `(log "DEBUG:" ~msg)))

;; Tree shaking - unused functions are removed
(defn ^:private internal-helper [x] 
  (* x 2))
```

### 8.2 Optimization Hints

```lisp
;; Type hints for performance
(defn fast-math ^double [^double x ^double y]
  (+ x y))

;; Inline functions
(definline square [x]
  `(* ~x ~x))

;; Memoization
(def fibonacci
  (memoize
    (fn [n]
      (if (<= n 1)
        n
        (+ (fibonacci (dec n))
           (fibonacci (- n 2)))))))

;; Transducers for efficient data processing
(transduce
  (comp (filter even?)
        (map #(* % %))
        (take 10))
  conj
  []
  (range 100))
```

### 8.3 Memory Management

```lisp
;; Lazy sequences for memory efficiency
(def infinite-numbers (iterate inc 0))
(take 10 infinite-numbers)  ; Only realizes what's needed

;; Efficient string building
(with-out-str
  (dotimes [i 1000]
    (print i " ")))

;; Resource management
(with-resource [file (open-file "data.txt")]
  (process-file file))  ; File automatically closed
```

---

## Advanced Features

### 9.1 Async Programming

```lisp
;; Promises/Futures
(def future-result
  (future
    (Thread/sleep 1000)
    "Computed value"))

@future-result  ; Blocks until ready

;; Core.async channels
(require '[clojure.core.async :as async])

(def ch (async/chan))

;; Producer
(async/go
  (async/>! ch "Hello")
  (async/>! ch "World"))

;; Consumer  
(async/go
  (let [msg (async/<! ch)]
    (log "Received:" msg)))

;; HTTP requests
(def-async fetch-user [user-id]
  (let [response (http/get (str "/api/users/" user-id))]
    (if (= 200 (:status response))
      (:body response)
      (throw (ex-info "User not found" {:user-id user-id})))))
```

### 9.2 Metaprogramming

```lisp
;; Code generation
(defmacro def-getter [name field]
  `(defn ~name [obj#]
     (~field obj#)))

(def-getter get-name :name)
(def-getter get-age :age)

;; Compile-time code analysis
(defmacro validate-fn [name params & body]
  (when (empty? params)
    (throw (ex-info "Function must have parameters" {:name name})))
  `(defn ~name ~params ~@body))

;; Runtime code evaluation
(eval '(+ 1 2 3))  ; => 6

;; Code walking and transformation
(walk/postwalk
  #(if (and (list? %) (= 'old-fn (first %)))
     (cons 'new-fn (rest %))
     %)
  '(old-fn 1 2 (old-fn 3 4)))
```

### 9.3 Testing Framework

```lisp
;; Unit tests
(deftest test-addition
  (is (= 4 (+ 2 2)))
  (is (= 0 (+ -1 1)))
  (is (thrown? ArithmeticException (/ 1 0))))

;; Property-based testing
(defspec addition-is-commutative 100
  (prop/for-all [a gen/int
                 b gen/int]
    (= (+ a b) (+ b a))))

;; UI component testing
(deftest test-counter-component
  (let [rendered (render (counter {:initial-value 5}))]
    (is (= "5" (text-content rendered ".count")))
    (click rendered "button.increment")
    (is (= "6" (text-content rendered ".count")))))

;; Integration tests
(deftest test-user-workflow
  (testing "User can login and view profile"
    (let [app (create-test-app)]
      (fill-form app "#login-form" {:username "alice" :password "secret"})
      (click app "#login-button")
      (wait-for app "#profile-page")
      (is (visible? app "#welcome-message")))))
```

### 9.4 Development Tools

```lisp
;; REPL integration
(defn repl-start []
  (require 'clojure.repl)
  (clojure.repl/repl))

;; Hot reloading
(defn ^:dev/after-load reload-hook []
  (log "Code reloaded!")
  (re-render-app))

;; Debugging
(defn debug [value]
  (log "DEBUG:" value)
  value)  ; Returns value for easy insertion

(-> data
    (process-step-1)
    debug           ; Log intermediate result
    (process-step-2)
    debug)

;; Performance profiling
(defmacro time-it [expr]
  `(let [start# (js/Date.now)
         result# ~expr
         end# (js/Date.now)]
     (log "Execution time:" (- end# start#) "ms")
     result#))

;; Usage
(time-it (expensive-computation data))
```

### 9.5 Custom Syntax Extensions

```lisp
;; Custom reader macros
(defn read-regex [reader letter]
  (let [pattern (read-until reader #{\"})]
    `(re-pattern ~pattern)))

(set-macro-character \# \r read-regex)
; Now #r"pattern" creates regex patterns

;; DSL creation
(defmacro html [& body]
  (walk/postwalk
    (fn [form]
      (if (and (vector? form) (keyword? (first form)))
        (let [[tag attrs & children] form
              attrs (if (map? attrs) attrs {})]
          `(element ~tag ~attrs ~@children))
        form))
    `(fragment ~@body)))

;; Usage
(html
  [:div {:class "container"}
    [:h1 {} "Welcome"]
    [:p {} "This is HTML-like syntax in Lisp!"]])
```

---

## Examples & Patterns

### 10.1 Complete Application Example

```lisp
(ns todo-app
  (:require [kryon.core :as k]
            [kryon.state :as state]))

;; State management
(def-atom todos [])
(def-atom filter-type :all) ; :all, :active, :completed
(def-atom new-todo-text "")

;; Helper functions
(defn add-todo [text]
  (when (not (empty? text))
    (swap! todos conj {:id (random-uuid)
                       :text text
                       :completed false})
    (reset! new-todo-text "")))

(defn toggle-todo [id]
  (swap! todos 
    (fn [todos]
      (map #(if (= (:id %) id)
              (update % :completed not)
              %)
           todos))))

(defn delete-todo [id]
  (swap! todos #(remove (fn [todo] (= (:id todo) id)) %)))

;; Filtered todos reaction
(def-reaction filtered-todos
  (let [filter @filter-type
        all-todos @todos]
    (case filter
      :all all-todos
      :active (filter #(not (:completed %)) all-todos)
      :completed (filter :completed all-todos))))

;; Components
(defui todo-item [{:keys [id text completed]}]
  (div {:class "todo-item"}
    (input {:type "checkbox"
            :checked completed
            :on-change #(toggle-todo id)})
    (span {:class (when completed "completed")} text)
    (button {:class "delete"
             :on-click #(delete-todo id)} "×")))

(defui todo-list []
  (div {:class "todo-list"}
    (for [todo @filtered-todos]
      (todo-item todo))))

(defui filter-buttons []
  (div {:class "filters"}
    (for [filter-option [:all :active :completed]]
      (button {:class (when (= @filter-type filter-option) "active")
               :on-click #(reset! filter-type filter-option)}
        (name filter-option)))))

(defui new-todo-input []
  (input {:type "text"
          :placeholder "What needs to be done?"
          :value @new-todo-text
          :on-change #(reset! new-todo-text (.-value (.-target %)))
          :on-key-press #(when (= "Enter" (.-key %))
                          (add-todo @new-todo-text))}))

(defui todo-app []
  (div {:class "todo-app"}
    (h1 {} "Todos")
    (new-todo-input)
    (todo-list)
    (filter-buttons)
    (div {:class "stats"}
      (span {} (count @filtered-todos) " items"))))

;; App initialization
(defn init []
  (k/render (todo-app) (js/document.getElementById "app")))

(init)
```

---

This comprehensive Kryon Lisp specification provides a modern, powerful Lisp dialect specifically designed for UI development and reactive programming. It combines traditional Lisp power with contemporary language features and seamless integration with the broader Kryon ecosystem.

The language supports everything from simple scripting to complex application development, with built-in support for reactive state management, UI components, and cross-platform development. Its interoperability features ensure it works seamlessly with JavaScript, KRY, and native platform APIs.