(function () {
  var storageKey = "deui-theme";

  function resolveTheme() {
    if (window.matchMedia("(prefers-color-scheme: no-preference)").matches) {
      return "dark";
    }
    return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
  }

  function applyTheme(theme) {
    var root = document.documentElement;
    var isLight = theme === "light";

    root.classList.remove("light", "dark");
    root.classList.add(theme);
    localStorage.setItem(storageKey, theme);

    var toggle = document.getElementById("theme-toggle");
    if (toggle) {
      toggle.setAttribute("aria-label", isLight ? "Switch to dark mode" : "Switch to light mode");
    }

    document.querySelectorAll(".theme-image__light").forEach(function (img) {
      img.setAttribute("aria-hidden", isLight ? "false" : "true");
    });

    document.querySelectorAll(".theme-image__dark").forEach(function (img) {
      img.setAttribute("aria-hidden", isLight ? "true" : "false");
    });
  }

  window.deuiApplyTheme = applyTheme;

  document.addEventListener("DOMContentLoaded", function () {
    var toggle = document.getElementById("theme-toggle");
    if (toggle) {
      toggle.addEventListener("click", function () {
        applyTheme(document.documentElement.classList.contains("light") ? "dark" : "light");
      });
    }

    window.matchMedia("(prefers-color-scheme: dark)").addEventListener("change", function () {
      if (!localStorage.getItem(storageKey)) {
        applyTheme(resolveTheme());
      }
    });

    applyTheme(document.documentElement.classList.contains("light") ? "light" : "dark");
  });
})();
