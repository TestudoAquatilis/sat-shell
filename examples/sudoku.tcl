# encode sudoku puzzle (puzzle example below) and constraints and solve

proc encode_sudoku_constraints {sub_size} {
    set size [expr $sub_size * $sub_size]

    # positions
    for {set i_line 1} {$i_line <= $size} {incr i_line} {
        for {set i_col 1} {$i_col <= $size} {incr i_col} {
            set lit_list [list]

            for {set i_num 1} {$i_num <= $size} {incr i_num} {
                lappend lit_list "field_${i_col}_${i_line}_${i_num}"
            }

            add_encoding -encoding "1ofn_order" -literals $lit_list
        }
    }

    # lines
    for {set i_line 1} {$i_line <= $size} {incr i_line} {
        for {set i_num 1} {$i_num <= $size} {incr i_num} {
            set lit_list [list]

            for {set i_col 1} {$i_col <= $size} {incr i_col} {
                lappend lit_list "field_${i_col}_${i_line}_${i_num}"
            }

            add_encoding -encoding "1ofn_order" -literals $lit_list
        }
    }

    # columns
    for {set i_col 1} {$i_col <= $size} {incr i_col} {
        for {set i_num 1} {$i_num <= $size} {incr i_num} {
            set lit_list [list]

            for {set i_line 1} {$i_line <= $size} {incr i_line} {
                lappend lit_list "field_${i_col}_${i_line}_${i_num}"
            }

            add_encoding -encoding "1ofn_order" -literals $lit_list
        }
    }

    # sub-fields
    for {set i_sub_line 0} {$i_sub_line < $sub_size} {incr i_sub_line} {
        for {set i_sub_col 0} {$i_sub_col < $sub_size} {incr i_sub_col} {
            for {set i_num 1} {$i_num <= $size} {incr i_num} {
                set lit_list [list]

                for {set i_line 1} {$i_line <= $sub_size} {incr i_line} {
                    for {set i_col 1} {$i_col <= $sub_size} {incr i_col} {
                        set col  [expr $sub_size * $i_sub_col + $i_col]
                        set line [expr $sub_size * $i_sub_line + $i_line]
                        lappend lit_list "field_${col}_${line}_${i_num}"
                    }
                }

                add_encoding -encoding "1ofn_order" -literals $lit_list
            }
        }
    }
}

proc encode_fixed_values {sub_size val_list} {
    set size [expr $sub_size * $sub_size]

    # positions
    for {set i_line 1} {$i_line <= $size} {incr i_line} {
        for {set i_col 1} {$i_col <= $size} {incr i_col} {
            set i_num [lindex $val_list [expr $i_line - 1] [expr $i_col - 1]]

            if {$i_num > 0} {
                add_clause -clause [list "field_${i_col}_${i_line}_${i_num}"]
            }
        }
    }
}

proc print_sudoku_result {sub_size} {
    set size [expr $sub_size * $sub_size]
    
    puts -nonewline " +-"
    for {set i_col 1} {$i_col <= $size} {incr i_col} {
            if {$i_col % $sub_size == 0} {
                if {$i_col < $size} {
                    puts -nonewline "---+-"
                } else {
                    puts -nonewline "---+ "
                }
            } else {
                puts -nonewline "----"
            }
    }
    puts ""

    for {set i_line 1} {$i_line <= $size} {incr i_line} {
        puts -nonewline " | "
        for {set i_col 1} {$i_col <= $size} {incr i_col} {
            for {set i_num 1} {$i_num <= $size} {incr i_num} {
                if {[get_var_result -var "field_${i_col}_${i_line}_${i_num}"]} {
                    puts -nonewline [format "%2d" $i_num]
                }
            }

            if {$i_col % $sub_size == 0} {
                puts -nonewline " | "
            } else {
                puts -nonewline "  "
            }
        }
        puts ""

        if {$i_line % $sub_size == 0} {
            puts -nonewline " +-"
            for {set i_col 1} {$i_col <= $size} {incr i_col} {
                    if {$i_col % $sub_size == 0} {
                        if {$i_col < $size} {
                            puts -nonewline "---+-"
                        } else {
                            puts -nonewline "---+ "
                        }
                    } else {
                        puts -nonewline "----"
                    }
            }
            puts ""
        }
    }
}


# 3 x 3 x 3 x 3 from wikipedia
set sub_size 3
set values [list \
    {0 3 0  0 0 0  0 0 0} \
    {0 0 0  1 9 5  0 0 0} \
    {0 0 8  0 0 0  0 6 0} \
    \
    {8 0 0  0 6 0  0 0 0} \
    {4 0 0  8 0 0  0 0 1} \
    {0 0 0  0 2 0  0 0 0} \
    \
    {0 6 0  0 0 0  2 8 0} \
    {0 0 0  4 1 9  0 0 5} \
    {0 0 0  0 0 0  0 7 0} \
]

encode_sudoku_constraints $sub_size
encode_fixed_values $sub_size $values
if {[solve]} {
    print_sudoku_result $sub_size
}
