/**
 * @name Unvalidated file upload without size or type checks
 * @description Detects file upload handlers that read uploaded content without
 *              validating file size, content type, or extension. The iccdev-mcp
 *              REST API accepts ICC profile uploads at /api/upload. Missing
 *              validation could allow memory exhaustion (DoS) or processing of
 *              non-ICC files that trigger unexpected behavior in analysis tools.
 * @kind problem
 * @problem.severity warning
 * @id iccdev-mcp/unvalidated-file-upload
 * @tags security file-upload dos input-validation iccdev
 * @cwe CWE-434
 */

import python

/**
 * File read calls inside upload handler functions.
 * Matches: upload.read(), request.form(), file.read()
 */
class FileReadCall extends Call {
  FileReadCall() {
    this.getFunc().(Attribute).getName() in ["read", "form"] and
    exists(Function f |
      f.getName() in [
        "api_upload", "api_output_download",
        "handle_upload", "upload_profile",
        "upload_and_analyze"
      ] and
      this.getScope() = f
    )
  }
}

/**
 * Size validation: len(content) compared against a limit.
 */
class SizeValidation extends Compare {
  SizeValidation() {
    exists(Call c |
      (
        c.getFunc().(Name).getId() = "len" or
        c.getFunc().(Attribute).getName() in [
          "content_length", "size"
        ]
      ) and
      this.getASubExpression() = c
    )
  }
}

/**
 * Content type or filename validation.
 */
class ContentTypeCheck extends Compare {
  ContentTypeCheck() {
    exists(Attribute a |
      a.getName() in ["content_type", "filename"] and
      this.getASubExpression() = a
    )
  }
}

from FileReadCall readCall, Function handler
where
  handler = readCall.getScope() and
  not exists(SizeValidation sv | sv.getScope() = handler) and
  not exists(ContentTypeCheck ctc | ctc.getScope() = handler)
select readCall,
  "File upload in " + handler.getName() +
  " reads content without size limit or content type validation. " +
  "Add MAX_UPLOAD_BYTES check and validate .icc/.icm extension."
