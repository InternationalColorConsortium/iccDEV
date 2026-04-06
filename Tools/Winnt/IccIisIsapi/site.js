document.addEventListener("DOMContentLoaded", () => {
  const resultUrl = document.getElementById("result-url");
  const resultCode = document.getElementById("result-code");
  const resultType = document.getElementById("result-type");
  const resultBody = document.getElementById("result-body");
  const liveStatus = document.getElementById("live-status");
  const footerYear = document.getElementById("footer-year");
  const toolForm = document.getElementById("tool-upload-form");
  const toolResults = document.getElementById("tool-results");
  const toolStatus = document.getElementById("tool-run-status");

  if (footerYear) {
    footerYear.textContent = String(new Date().getFullYear());
  }

  async function loadEndpoint(relativeUrl) {
    if (!resultBody) {
      return;
    }

    // Sanitize the URL: strip fragment (#), reject non-http(s) schemes.
    // Defense against DOM-XSS via attacker-controlled endpoint attributes
    // or future code that reads location.hash / location.search.
    const safeUrl = typeof iccSanitize !== "undefined"
      ? iccSanitize.sanitizeUri(relativeUrl)
      : relativeUrl;
    if (!safeUrl) {
      if (resultBody) resultBody.textContent = "Blocked: unsafe URL";
      return;
    }

    const absoluteUrl = new URL(safeUrl, window.location.href).toString();
    if (resultUrl) {
      resultUrl.textContent = absoluteUrl;
    }
    if (resultCode) {
      resultCode.textContent = "Loading";
    }
    if (resultType) {
      resultType.textContent = "Pending";
    }
    resultBody.textContent = "Loading " + absoluteUrl + " ...";

    if (liveStatus) {
      liveStatus.className = "status-chip";
      liveStatus.textContent = "Request in flight";
    }

    try {
      const response = await fetch(relativeUrl, {
        headers: {
          "Accept": "text/plain, application/xml, text/xml;q=0.9, */*;q=0.8"
        }
      });
      const text = await response.text();

      if (resultCode) {
        resultCode.textContent = response.status + " " + response.statusText;
      }
      if (resultType) {
        resultType.textContent = response.headers.get("content-type") || "n/a";
      }
      resultBody.textContent = text;

      if (liveStatus) {
        liveStatus.className = response.ok ? "status-chip ok" : "status-chip fail";
        liveStatus.textContent = response.ok ? "Request completed successfully" : "Request returned an error";
      }
    } catch (error) {
      if (resultCode) {
        resultCode.textContent = "Fetch failed";
      }
      if (resultType) {
        resultType.textContent = "n/a";
      }
      resultBody.textContent = String(error);

      if (liveStatus) {
        liveStatus.className = "status-chip fail";
        liveStatus.textContent = "Browser fetch failed";
      }
    }
  }

  document.querySelectorAll("[data-endpoint-url]").forEach((button) => {
    button.addEventListener("click", () => {
      loadEndpoint(button.getAttribute("data-endpoint-url"));
    });
  });

  function createToolMetaItem(list, label, value) {
    const dt = document.createElement("dt");
    dt.textContent = label;
    const dd = document.createElement("dd");
    dd.textContent = value;
    list.appendChild(dt);
    list.appendChild(dd);
  }

  function createToolLink(href, label) {
    const link = document.createElement("a");
    // Sanitize href from server JSON — block javascript:/data: schemes
    // and strip fragment payloads that could enable DOM-XSS.
    const safeHref = typeof iccSanitize !== "undefined"
      ? iccSanitize.sanitizeUri(href)
      : href;
    link.href = safeHref || "#";
    link.textContent = label;
    link.target = "_blank";
    link.rel = "noreferrer noopener";
    link.className = "tool-link";
    return link;
  }

  function renderToolResults(payload) {
    if (!toolResults) {
      return;
    }

    toolResults.textContent = "";

    if (payload.error) {
      const errorBlock = document.createElement("div");
      errorBlock.className = "tool-result fail";
      const heading = document.createElement("h3");
      heading.textContent = "Tool suite error";
      const body = document.createElement("pre");
      body.className = "code-block";
      body.textContent = payload.error;
      errorBlock.appendChild(heading);
      errorBlock.appendChild(body);
      toolResults.appendChild(errorBlock);
      return;
    }

    const summary = document.createElement("div");
    summary.className = "tool-summary";
    const summaryHeading = document.createElement("h3");
    summaryHeading.textContent = "Upload summary";
    const summaryMeta = document.createElement("dl");
    summaryMeta.className = "tool-meta";
    createToolMetaItem(summaryMeta, "Input kind", payload.input?.kind || "n/a");
    createToolMetaItem(summaryMeta, "Filename", payload.input?.filename || "n/a");
    createToolMetaItem(summaryMeta, "Bytes", String(payload.input?.bytes ?? "n/a"));
    summary.appendChild(summaryHeading);
    summary.appendChild(summaryMeta);

    const summaryLinks = document.createElement("div");
    summaryLinks.className = "tool-links";
    if (payload.input?.url) {
      summaryLinks.appendChild(createToolLink(payload.input.url, "Open uploaded input"));
    }
    if (payload.workspace_url) {
      summaryLinks.appendChild(createToolLink(payload.workspace_url, "Open workspace directory"));
    }
    if (summaryLinks.childElementCount) {
      summary.appendChild(summaryLinks);
    }
    toolResults.appendChild(summary);

    (payload.tools || []).forEach((tool) => {
      const block = document.createElement("div");
      block.className = "tool-result " + (tool.skipped ? "skipped" : (tool.ok ? "ok" : "fail"));

      const heading = document.createElement("h3");
      heading.textContent = tool.name;
      block.appendChild(heading);

      const meta = document.createElement("dl");
      meta.className = "tool-meta";
      createToolMetaItem(meta, "Status", tool.skipped ? "Skipped" : (tool.ok ? "Succeeded" : "Failed"));
      createToolMetaItem(meta, "Exit code", String(tool.exit_code));
      createToolMetaItem(meta, "Command", tool.command || "n/a");
      if (tool.note) {
        createToolMetaItem(meta, "Note", tool.note);
      }
      if (tool.artifact_name) {
        createToolMetaItem(meta, "Artifact", tool.artifact_name);
        createToolMetaItem(meta, "Artifact bytes", String(tool.artifact_bytes ?? 0));
      }
      if (tool.log_name) {
        createToolMetaItem(meta, "Log", tool.log_name);
        createToolMetaItem(meta, "Log bytes", String(tool.log_bytes ?? 0));
      }
      block.appendChild(meta);

      const links = document.createElement("div");
      links.className = "tool-links";
      if (tool.artifact_url) {
        links.appendChild(createToolLink(tool.artifact_url, "Open artifact"));
      }
      if (tool.log_url) {
        links.appendChild(createToolLink(tool.log_url, "Open log"));
      }
      if (links.childElementCount) {
        block.appendChild(links);
      }

      const output = document.createElement("pre");
      output.className = "code-block";
      output.textContent = tool.output || "No output captured.";
      block.appendChild(output);

      if (tool.artifact_preview) {
        const previewHeading = document.createElement("h3");
        previewHeading.textContent = "Artifact preview";
        previewHeading.style.marginTop = "14px";
        const preview = document.createElement("pre");
        preview.className = "code-block";
        preview.textContent = tool.artifact_preview;
        block.appendChild(previewHeading);
        block.appendChild(preview);
      }

      toolResults.appendChild(block);
    });
  }

  if (toolForm) {
    toolForm.addEventListener("submit", async (event) => {
      event.preventDefault();

      const fileInput = document.getElementById("tool-upload-file");
      const inputKind = document.getElementById("tool-upload-kind");
      const file = fileInput?.files?.[0];

      if (!file) {
        if (toolStatus) {
          toolStatus.className = "status-chip fail";
          toolStatus.textContent = "Choose an ICC or XML file first";
        }
        return;
      }

      const kind = inputKind?.value === "xml" ? "xml" : "icc";
      const targetUrl = `./iccIisIsapi.dll?mode=tools&input=${encodeURIComponent(kind)}&filename=${encodeURIComponent(file.name)}`;

      if (toolStatus) {
        toolStatus.className = "status-chip";
        toolStatus.textContent = "Running top 4 tools";
      }
      if (toolResults) {
        toolResults.textContent = "";
      }

      try {
        const response = await fetch(targetUrl, {
          method: "POST",
          headers: {
            "Accept": "application/json",
            "Content-Type": kind === "xml" ? "application/xml" : "application/octet-stream"
          },
          body: file
        });

        const text = await response.text();
        let payload;
        try {
          payload = JSON.parse(text);
        } catch (error) {
          payload = { error: text || String(error) };
        }

        renderToolResults(payload);

        if (toolStatus) {
          toolStatus.className = response.ok && !payload.error ? "status-chip ok" : "status-chip fail";
          toolStatus.textContent = response.ok && !payload.error
            ? "Tool suite completed"
            : "Tool suite returned an error";
        }
      } catch (error) {
        renderToolResults({ error: String(error) });
        if (toolStatus) {
          toolStatus.className = "status-chip fail";
          toolStatus.textContent = "Browser upload failed";
        }
      }
    });
  }

  const autoEndpoint = document.body.dataset.autoEndpoint;
  if (autoEndpoint && resultBody) {
    // Sanitize data attribute value before using as fetch URL.
    const safeAutoUrl = typeof iccSanitize !== "undefined"
      ? iccSanitize.sanitizeUri(autoEndpoint)
      : autoEndpoint;
    if (safeAutoUrl) {
      loadEndpoint(safeAutoUrl);
    }
  }
});
