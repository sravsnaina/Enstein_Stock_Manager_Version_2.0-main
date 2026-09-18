pragma Singleton
import QtQuick

// =============================================================================
//  Format - turning numbers and dates into what the user should read
// =============================================================================
//  Pure functions, no state, no bindings to anything. Every screen formats
//  money and quantities through here so an amount looks the same on the stock
//  grid, in a purchase order and on a printed challan.
//
//  Money is grouped the Indian way (12,34,56,789.00) to match the printed
//  documents, which is why this cannot simply use toLocaleString().
//
//  Usage:  import "../util"      then   Format.rupees(value)
// =============================================================================
QtObject {

    // Anything unparseable reads as 0 rather than NaN, because these values
    // come from text fields the user is still typing into.
    function amount(value) {
        var n = parseFloat(value)
        return isNaN(n) ? 0 : n
    }

    // Two decimal places, no grouping and no symbol: for text fields the user
    // will edit, where separators would have to be stripped again.
    function fixed(value) {
        return amount(value).toFixed(2)
    }

    // Indian digit grouping: the last three digits, then pairs above that.
    function grouped(value) {
        var n = amount(value)
        var text = Math.abs(n).toFixed(2)
        var dot = text.indexOf(".")
        var whole = text.substring(0, dot)
        var frac = text.substring(dot)
        if (whole.length > 3) {
            var last3 = whole.substring(whole.length - 3)
            var lead = whole.substring(0, whole.length - 3)
            var out = ""
            while (lead.length > 2) {
                out = "," + lead.substring(lead.length - 2) + out
                lead = lead.substring(0, lead.length - 2)
            }
            whole = lead + out + "," + last3
        }
        return (n < 0 ? "-" : "") + whole + frac
    }

    // With the rupee sign. For display only.
    function rupees(value) {
        return "₹ " + grouped(value)
    }

    // Quantities are usually whole but not always, so a round number shows
    // without a trailing ".00" and a fractional one keeps two places.
    function qty(value) {
        var n = amount(value)
        return (Math.abs(n - Math.round(n)) < 0.005)
                ? Math.round(n).toString() : n.toFixed(2)
    }

    // "from -> to" for a required period, collapsing to one date when only one
    // end is known. Mirrors expectedPeriod() on the printed order.
    function period(fromText, toText) {
        var from = (fromText || "").toString().trim()
        var to = (toText || "").toString().trim()
        if (from === "" && to === "") return "-"
        if (to === "" || from === to) return from === "" ? to : from
        if (from === "") return "up to " + to
        return from + " → " + to
    }

    // Inclusive length of the period, or 0 when it is not a usable range.
    function periodDays(fromText, toText) {
        var from = new Date(fromText)
        var to = new Date(toText)
        if (isNaN(from.getTime()) || isNaN(to.getTime())) return 0
        var days = Math.round((to.getTime() - from.getTime()) / 86400000) + 1
        return days > 0 ? days : 0
    }
}
