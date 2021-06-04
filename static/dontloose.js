function dontLoose() {
  window.onbeforeunload = function (e) {
    e = e || window.event;
    let txt = "If you continue, you might loose data!";

    // For IE and Firefox prior to version 4
    if (e) {
      e.returnValue = txt;
    }

    // For Safari
    return txt;
  };
}

function loose() {
    window.onbeforeunload = null;
}
