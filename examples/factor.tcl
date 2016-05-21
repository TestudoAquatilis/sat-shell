# split a number into two factors:
# encode a simple integer multiplier, fix the product
# and search for possible factors where factor must be < product.
# if unsatisfiable, number must be prime

proc encode_product {number} {
    set n_bits_product 0

    set product $number

    while {$product > 0} {
        if {$product % 2 == 1} {
            add_clause -clause [list "p_${n_bits_product}"]
        } else {
            add_clause -clause [list "-p_${n_bits_product}"]
        }

        incr n_bits_product
        set product [expr $product / 2]
    }

    return $n_bits_product
}

proc gen_first_line_simple {len_f1 {last "false"}} {
    for {set i 0} {$i < $len_f1} {incr i} {
        if {$last} {
            set i_sum "p_$i"
        } else {
            set i_sum "inter_0_$i"
        }
        add_formula -formula "1 == 2 and 3" -mapping [list "${i_sum}" "f1_${i}" "f2_0"]
    }
}

proc gen_first_line_short {len_f1 {last "false"}} {
    # lsb
    add_formula -formula "1 == 2 and 3" -mapping [list "p_0" "f1_0" "f2_0"]
    if {$len_f1 <= 1} {return}

    # bit 1
    add_formula -formula "1 == (2 and 3) xor (4 and 5)" \
                -mapping [list "p_1" "f1_1" "f2_0" "f1_0" "f2_1"]
    add_formula -formula "1 == 2 and 3 and 4 and 5" \
                -mapping [list "inter_1_carry_1" "f1_1" "f2_0" "f1_0" "f2_1"]

    # intermediate bits
    for {set i 2} {$i < $len_f1} {incr i} {
        set im1 [expr $i - 1]

        if {$last} {
            set i_sum "p_$i"
        } else {
            set i_sum "inter_1_$i"
        }

        add_formula -formula "1 == (2 and 3) xor (4 and 5) xor 6" \
                    -mapping [list "$i_sum" "f1_$i" "f2_0" "f1_${im1}" "f2_1" "inter_1_carry_${im1}"]
        add_formula -formula "1 == (2 and 3) and (4 and 5) or (2 and 3) and 6 or (4 and 5) and 6" \
                    -mapping [list "inter_1_carry_$i" "f1_$i" "f2_0" "f1_${im1}" "f2_1" "inter_1_carry_${im1}"]
    }

    # last 2 bits
    set i $len_f1
    set im1 [expr $i - 1]
    if {$last} {
        set i_sum "p_$i"
        set ip1_sum "p_[expr $i + 1]"
    } else {
        set i_sum "inter_1_$i"
        set ip1_sum "inter_1_[expr $i + 1]"
    }
    add_formula -formula "1 == (2 and 3) xor 4" \
                -mapping [list "$i_sum" "f1_${im1}" "f2_1" "inter_1_carry_${im1}"]
    add_formula -formula "1 == 2 and 3 and 4" \
                -mapping [list "$ip1_sum" "f1_${im1}" "f2_1" "inter_1_carry_${im1}"]
}

proc gen_line_short {len_f1 i_line {last "false"}} {
    set prev_line [expr $i_line - 1]
    # first bit
    if {$last && ($len_f1 < 2)} {
        set i_c "p_[expr $i + 1]"
    } else {
        set i_c "inter_${i_line}_carry_${i_line}"
    }
    add_formula -formula "1 == (2 and 3) xor 4" \
                -mapping [list "p_${i_line}" "f1_0" "f2_${i_line}" "inter_${prev_line}_${i_line}"]
    add_formula -formula "1 == 2 and 3 and 4" \
                -mapping [list "$i_c" "f1_0" "f2_${i_line}" "inter_${prev_line}_${i_line}"]

    # intermediate bits
    for {set i 1} {$i < $len_f1 - 1} {incr i} {
        set bitpos [expr $i + $i_line]
        set bitposm1 [expr $bitpos - 1]
        if {$last} {
            set i_sum "p_$bitpos"
        } else {
            set i_sum "inter_${i_line}_$bitpos"
        }

        add_formula -formula "1 == (2 and 3) xor 4 xor 5" \
                    -mapping [list "$i_sum" "f1_$i" "f2_${i_line}" "inter_${prev_line}_${bitpos}" "inter_${i_line}_carry_${bitposm1}"]
        add_formula -formula "1 == (2 and 3) and 4 or (2 and 3) and 5 or 4 and 5" \
                    -mapping [list "inter_${i_line}_carry_${bitpos}" "f1_$i" "f2_${i_line}" "inter_${prev_line}_${bitpos}" "inter_${i_line}_carry_${bitposm1}"]
    }

    # last 2 bits
    set i [expr $len_f1 - 1]
    set bitpos [expr $i + $i_line]
    set bitposm1 [expr $bitpos - 1]

    if {$last} {
        set i_sum "p_$bitpos"
        set ip1_sum "p_[expr $bitpos + 1]"
    } else {
        set i_sum "inter_${i_line}_$bitpos"
        set ip1_sum "inter_${i_line}_[expr $bitpos + 1]"
    }
    add_formula -formula "1 == (2 and 3) xor 4 xor 5" \
                -mapping [list "$i_sum" "f1_$i" "f2_${i_line}" "inter_${prev_line}_${bitpos}" "inter_${i_line}_carry_${bitposm1}"]
    add_formula -formula "1 == (2 and 3) and 4 or (2 and 3) and 5 or 4 and 5" \
                -mapping [list "$ip1_sum" "f1_$i" "f2_${i_line}" "inter_${prev_line}_${bitpos}" "inter_${i_line}_carry_${bitposm1}"]
}

proc get_number_result {prefix len} {
    set result 0

    for {set i [expr $len - 1]} {$i >= 0} {set i [expr $i - 1]} {
        set result [expr $result * 2]

        if {[get_var_result -var "${prefix}${i}"]} {
            set result [expr $result + 1]
        }
    }

    return $result
}

proc encode_factor {number} {
    set n_bits_product [encode_product $number]

    # factors
    set n_bits_f1 [expr $n_bits_product - 1]
    set n_bits_f2 [expr int(ceil($n_bits_product / 2.0))]

    # first line
    if {$n_bits_f1 <= 1} {
        gen_first_line_simple $n_bits_f1 [expr $n_bits_f2 < 2]
    } else {
        gen_first_line_short $n_bits_f1 [expr $n_bits_f2 < 3]
    }


    for {set i 2} {$i < ($n_bits_f2 - 1)} {incr i} {
        gen_line_short $n_bits_f1 $i "false"
    }

    gen_line_short $n_bits_f1 [expr $n_bits_f2 - 1] "true"

    return [list $n_bits_product $n_bits_f1 $n_bits_f2]
}

proc factor {number} {
    set len_list [encode_factor $number]

    set p_len [lindex $len_list 0]
    set p_len_max [expr [lindex $len_list 1] + [lindex $len_list 2]]

    # topmost product bits = 0
    for {set i $p_len} {$i < $p_len_max} {incr i} {
        add_clause -clause [list "-p_${i}"]
    }

    if {[solve]} {
        set f1 [get_number_result "f1_" [lindex $len_list 1]]
        set f2 [get_number_result "f2_" [lindex $len_list 2]]

        puts "$number = $f1 * $f2"
    } else {
        puts "$number is possibly prime"
    }

    reset
}

set number 2147483729
#set number 2147483693

factor $number

