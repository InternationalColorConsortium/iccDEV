document.addEventListener("DOMContentLoaded", () => {
  const resultUrl = document.getElementById("result-url");
  const resultCode = document.getElementById("result-code");
  const resultType = document.getElementById("result-type");
  const resultBody = document.getElementById("result-body");
  const liveStatus = document.getElementById("live-status");

  async function loadEndpoint(relativeUrl) {
    const safeUrl = typeof iccSanitize !== "undefined"
      ? iccSanitize.sanitizeUri(relativeUrl)
      : relativeUrl;
    if (!safeUrl) {
      resultBody.textContent = "Blocked: unsafe URL";
      return;
    }

    const absoluteUrl = new URL(safeUrl, window.location.href).toString();
    resultUrl.textContent = "URL: " + absoluteUrl;
    resultCode.textContent = "Status: loading";
    resultType.textContent = "Content-Type: pending";
    resultBody.textContent = "Loading " + absoluteUrl + " ...";
    liveStatus.className = "status";
    liveStatus.lastElementChild.textContent = "Request in flight.";

    try {
      const response = await fetch(relativeUrl, {
        headers: {
          "Accept": "text/plain, application/xml, text/xml;q=0.9, */*;q=0.8"
        }
      });
      const body = await response.text();
      resultCode.textContent = "Status: " + response.status + " " + response.statusText;
      resultType.textContent = "Content-Type: " + (response.headers.get("content-type") || "n/a");
      resultBody.textContent = body;
      liveStatus.className = response.ok ? "status ok" : "status fail";
      liveStatus.lastElementChild.textContent = response.ok
        ? "Request completed successfully."
        : "Request returned an error.";
    } catch (error) {
      resultCode.textContent = "Status: fetch failed";
      resultType.textContent = "Content-Type: n/a";
      resultBody.textContent = String(error);
      liveStatus.className = "status fail";
      liveStatus.lastElementChild.textContent = "Browser fetch failed.";
    }
  }

  document.getElementById("load-summary").addEventListener("click", () => loadEndpoint("./iccIisIsapi.dll"));
  document.getElementById("load-health").addEventListener("click", () => loadEndpoint("./iccIisIsapi.dll?mode=health"));
  document.getElementById("load-xml").addEventListener("click", () => loadEndpoint("./iccIisIsapi.dll?format=xml"));

  loadEndpoint("./iccIisIsapi.dll");
});
