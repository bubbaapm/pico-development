window.setTimeout(() => {
  const frame = document.getElementById("frame");
  if (frame) {
    const refresh = () => {
      frame.src =
          `http://${window.location.hostname}:8080/frame.bmp?t=${Date.now()}`;
    };
    refresh();
    window.setInterval(refresh, 1000);
  }

  const forms = document.querySelectorAll("form");
  forms.forEach((form) => {
    form.addEventListener("submit", () => {
      const button = form.querySelector("button");
      if (button) {
        button.textContent = "Sent";
      }
    });
  });
}, 0);
