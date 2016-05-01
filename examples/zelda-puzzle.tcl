set width 13
set height 9
set start {13 5}
set end {0 0}
set blocks [list {4 6} {7 3} {7 6} {7 8} {9 6} {7 7}]

proc print_zeldaproblem {{solution "false"}} {
    variable width
    variable height
    variable start
    variable end
    variable blocks

    for {set y $height} {$y > 0} {set y [expr $y - 1]} {
        if {$y < $height} {
            for {set x 1} {$x <= $width} {incr x} {
                if {! $solution} {
                    puts -nonewline " "
                } else {
                    if {[get_var_result -var "route_y_${x}_${y}_[expr $y + 1]"]} {
                        puts -nonewline "|"
                    } else {
                        puts -nonewline " "
                    }
                }
                if {$x < $width} {
                    puts -nonewline " "
                }
            }
            puts ""
        }
        for {set x 1} {$x <= $width} {incr x} {
            set coord [list $x $y]
            if {$solution} {
                if {[get_var_result -var "start_${x}_${y}"]} {
                    puts -nonewline "S"
                } elseif {[get_var_result -var "end_${x}_${y}"]} {
                    puts -nonewline "E"
                } elseif {[lsearch $blocks $coord] >= 0} {
                    puts -nonewline "X"
                } else {
                    puts -nonewline "#"
                }
            } else {
                if {$coord == $start} {
                    puts -nonewline "S"
                } elseif {$coord == $end} {
                    puts -nonewline "E"
                } elseif {[lsearch $blocks $coord] >= 0} {
                    puts -nonewline "X"
                } else {
                    puts -nonewline "#"
                }
            }
            if {$x < $width} {
                if {! $solution} {
                    puts -nonewline " "
                } else {
                    if {[get_var_result -var "route_x_${x}_[expr $x + 1]_${y}"]} {
                        puts -nonewline "-"
                    } else {
                        puts -nonewline " "
                    }
                }
            }
        }
        puts ""
    }
}

proc zelda_encode_field {x y} {
    variable width
    variable height
    variable start
    variable end
    variable blocks

    set var_list [list "start_${x}_${y}" "end_${x}_${y}"]
    if {$x > 1}       {lappend var_list "route_x_[expr $x - 1]_${x}_${y}"}
    if {$x < $width}  {lappend var_list "route_x_${x}_[expr $x + 1]_${y}"}
    if {$y > 1}       {lappend var_list "route_y_${x}_[expr $y - 1]_${y}"}
    if {$y < $height} {lappend var_list "route_y_${x}_${y}_[expr $y + 1]"}

    set coord [list $x $y]

    if {[lsearch $blocks $coord] >= 0} {
        foreach var $var_list {
            add_clause -clause [list "-$var"]
        }
    } else {
        add_encoding -encoding "2ofn" -literals $var_list
    }

    if {$coord == $start} {
        add_clause -clause [list "start_${x}_${y}"]
    }
    if {$coord == $end} {
        add_clause -clause [list "end_${x}_${y}"]
    }
}

proc zelda_encode_tfield {x y t} {
    variable width
    variable height
    variable start
    variable end
    variable blocks

    if {$t == 1} {
        return
    }

    set xyt "xyt_${x}_${y}_${t}"

    set coord [list $x $y]

    if {[lsearch $blocks $coord] >= 0} {
        add_clause -clause [list "-${xyt}"]
        return
    }

    if {$t == 2} {
        set xyt_prev "start_${x}_${y}"
    } else {
        set xyt_prev "xyt_${x}_${y}_[expr $t - 1]"
    }

    # xyt_prev -> xyt
    add_clause -clause [list "-${xyt_prev}" "$xyt"]

    set xyt_other_list [list]
    set route_list     [list]

    # xyt other prev AND route -> xyt
    if {$x > 1} {
        if {$t == 2} {
            set xyt_other_prev "start_[expr $x - 1]_${y}"
        } else {
            set xyt_other_prev "xyt_[expr $x - 1]_${y}_[expr $t - 1]"
        }
        set route_other    "route_x_[expr $x - 1]_${x}_${y}"
        lappend xyt_other_list $xyt_other_prev
        lappend route_list     $route_other
        add_clause -clause [list "-$xyt_other_prev" "-$route_other" "$xyt"]
    }
    if {$x < $width} {
        if {$t == 2} {
            set xyt_other_prev "start_[expr $x + 1]_${y}"
        } else {
            set xyt_other_prev "xyt_[expr $x + 1]_${y}_[expr $t - 1]"
        }
        set route_other    "route_x_${x}_[expr $x + 1]_${y}"
        lappend xyt_other_list $xyt_other_prev
        lappend route_list     $route_other
        add_clause -clause [list "-$xyt_other_prev" "-$route_other" "$xyt"]
    }
    if {$y > 1} {
        if {$t == 2} {
            set xyt_other_prev "start_${x}_[expr $y - 1]"
        } else {
            set xyt_other_prev "xyt_${x}_[expr $y - 1]_[expr $t - 1]"
        }
        set route_other    "route_y_${x}_[expr $y - 1]_${y}"
        lappend xyt_other_list $xyt_other_prev
        lappend route_list     $route_other
        add_clause -clause [list "-$xyt_other_prev" "-$route_other" "$xyt"]
    }
    if {$y < $height} {
        if {$t == 2} {
            set xyt_other_prev "start_${x}_[expr $y + 1]"
        } else {
            set xyt_other_prev "xyt_${x}_[expr $y + 1]_[expr $t - 1]"
        }
        set route_other    "route_y_${x}_${y}_[expr $y + 1]"
        lappend xyt_other_list $xyt_other_prev
        lappend route_list     $route_other
        add_clause -clause [list "-$xyt_other_prev" "-$route_other" "$xyt"]
    }

    # all others (prev 0 OR route 0) AND prev 0 -> xyt 0
    set n_routes [llength $route_list]
    for {set i 0} {$i < [expr 2 ** $n_routes]} {incr i} {
        set clause [list $xyt_prev "-$xyt"]
        for {set j 0} {$j  < $n_routes} {incr j} {
            if {($i / (2 ** $j)) % 2 == 0} {
                lappend clause [lindex $xyt_other_list $j]
            } else {
                lappend clause [lindex $route_list $j]
            }
        }

        add_clause -clause $clause
    }

    # last step: xyt = true
    set t_max [expr $width * $height - [llength $blocks]]
    if {$t == $t_max} {
        add_clause -clause [list $xyt]
    }
}

proc zelda_encode {} {
    variable width
    variable height
    variable start
    variable end
    variable blocks

    set start_list [list ]
    set end_list [list]

    set t_max [expr $width * $height - [llength $blocks]]

    for {set y 1} {$y <= $height} {incr y} {
        puts "encoding y = $y"
        for {set x 1} {$x <= $width} {incr x} {
            zelda_encode_field $x $y
            lappend start_list [list "start_${x}_${y}"]
            lappend end_list   [list "end_${x}_${y}"]
            for {set t 2} {$t <= $t_max} {incr t} {
                zelda_encode_tfield $x $y $t
            }
        }
    }

    add_encoding -encoding "1ofn_order" -literals $start_list
    add_encoding -encoding "1ofn_order" -literals $end_list
}

print_zeldaproblem "false"

zelda_encode

if {[solve]} {
    print_zeldaproblem "true"
}
