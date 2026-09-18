pragma Singleton
import QtQuick

// Keeps department spelling consistent across master-data forms and documents.
QtObject {
    readonly property var standard: [
        "Administration", "Electrical", "Electronics", "Mechanical",
        "Production", "Quality", "Stores"
    ]

    function options(items) {
        var result = standard.slice()
        for (var i = 0; items && i < items.length; ++i) {
            var value = items[i]
            var name = typeof value === "string" ? value
                       : (value.department || value.category || "")
            name = canonical(name)
            if (name !== "" && result.indexOf(name) === -1) result.push(name)
        }
        return result.sort()
    }

    function canonical(value) {
        var name = (value || "").toString().trim()
        if (name === "") return ""
        for (var i = 0; i < standard.length; ++i)
            if (standard[i].toLowerCase() === name.toLowerCase()) return standard[i]
        return name.replace(/\b\w/g, function(letter) { return letter.toUpperCase() })
    }

    function indexOf(options, department) { return options.indexOf(canonical(department)) }
}
