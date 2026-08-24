classdef IccApply < handle
  %ICCAPPLY Thread-safe apply handle for pixel transforms.
  %
  %   Obtained from IccCmm.get_apply(). Each thread/worker should
  %   have its own IccApply instance for concurrent transforms.
  %
  %   ah = cmm.get_apply();
  %   result = ah.apply([0.5 0.3 0.1]);
  %   ah.close();
  %
  % Copyright (c) International Color Consortium.
  % BSD 3-Clause License. See LICENSE.md for details.

  properties (SetAccess = private)
    Handle = uint64(0)
    SrcChannels = int32(0)
    DstChannels = int32(0)
    ParentCmm = []
  end

  methods
    function obj = IccApply(cmm, srcCh, dstCh)
      %ICCAPPLY Create from a CMM object (internal use - call IccCmm.get_apply).
      if nargin < 3
        error('iccdev:applyFactoryRequired', ...
          ['Create apply handles from an initialized CMM. Example: ' ...
           'apply_handle = cmm.get_apply();']);
      end
      if ~isa(cmm, 'iccdev.IccCmm') || ~cmm.is_valid()
        error('iccdev:invalidCmm', 'Parent CMM is closed or invalid.');
      end
      obj.ParentCmm = cmm;
      obj.Handle = icc_mex('apply_create', uint64(cmm.Handle));
      obj.SrcChannels = int32(srcCh);
      obj.DstChannels = int32(dstCh);
    end

    function result = apply(obj, pixels)
      %APPLY Transform pixels using the thread-safe handle.
      %   result = ah.apply([0.5 0.3 0.1])
      %   results = ah.apply(rand(100, 3))
      if obj.Handle == uint64(0)
        error('iccdev:closedHandle', 'Apply handle is closed.');
      end
      if isempty(obj.ParentCmm) || ~obj.ParentCmm.is_valid()
        error('iccdev:parentClosed', 'Parent CMM is closed.');
      end
      result = icc_mex('apply_apply', obj.Handle, pixels, ...
        obj.SrcChannels, obj.DstChannels);
    end

    function close(obj)
      %CLOSE Release the apply handle.
      if obj.Handle ~= uint64(0)
        icc_mex('apply_free', obj.Handle);
        obj.Handle = uint64(0);
      end
      obj.ParentCmm = [];
    end

    function delete(obj)
      obj.close();
    end
  end
end
