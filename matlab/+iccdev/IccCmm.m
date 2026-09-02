classdef IccCmm < handle
  %ICCCMM ICC Color Management Module - multi-profile transform pipeline.
  %
  %   repo_root = fileparts(fileparts(which('build_mex')));
  %   profile_path = fullfile(repo_root, 'Testing', ...
  %     'sRGB_v4_ICC_preference.icc');
  %   cmm = iccdev.IccCmm();
  %   cmm.attach(profile_path);
  %   cmm.attach(profile_path);
  %   cmm.begin();
  %   result = cmm.apply([0.5 0.3 0.1]);
  %   cmm.close();
  %
  %   % Bulk transform (N x channels matrix):
  %   pixels = rand(1000, 3, 'single');
  %   results = cmm.apply(pixels);
  %
  % Copyright (c) International Color Consortium.
  % BSD 3-Clause License. See LICENSE.md for details.

  properties (SetAccess = private)
    Handle = uint64(0)
    SrcChannels = int32(0)
    DstChannels = int32(0)
    IsReady = false
  end

  methods
    function obj = IccCmm(srcSpace, dstSpace, firstIsInput)
      %ICCCMM Create a Color Management Module.
      %   cmm = iccdev.IccCmm()
      %   cmm = iccdev.IccCmm(srcSpace, dstSpace, firstIsInput)
      if nargin < 1, srcSpace = double(hex2dec('3f3f3f3f')); end
      if nargin < 2, dstSpace = double(hex2dec('3f3f3f3f')); end
      if nargin < 3, firstIsInput = true; end
      obj.Handle = icc_mex('cmm_create', ...
        int32(srcSpace), int32(dstSpace), int32(firstIsInput));
    end

    function status = attach(obj, filename, varargin)
      %ATTACH Attach an ICC profile to the pipeline.
      %   cmm.attach('profile.icc')
      %   cmm.attach('profile.icc', 'intent', 0, 'interp', 1)
      %   cmm.attach('profile.icc', 'luttype', 0)  % valid range: 0..13
      intent  = int32(0);
      interp  = int32(0);
      lutType = int32(0);
      useD2B  = true;
      useBPC  = false;
      for k = 1:2:numel(varargin)
        switch lower(varargin{k})
          case 'intent',  intent  = int32(varargin{k+1});
          case 'interp',  interp  = int32(varargin{k+1});
          case 'luttype', lutType = int32(varargin{k+1});
          case 'used2b',  useD2B  = logical(varargin{k+1});
          case 'usebpc',  useBPC  = logical(varargin{k+1});
        end
      end
      if obj.IsReady
        error('iccdev:attachAfterBegin', 'Cannot attach after begin().');
      end
      status = icc_mex('cmm_attach', obj.Handle, char(filename), ...
        intent, interp, lutType, int32(useD2B), int32(useBPC));
    end

    function begin(obj)
      %BEGIN Initialize the transform pipeline. Call after all profiles attached.
      icc_mex('cmm_begin', obj.Handle);
      info = icc_mex('cmm_info', obj.Handle);
      obj.SrcChannels = int32(info.srcSamples);
      obj.DstChannels = int32(info.dstSamples);
      obj.IsReady = true;
    end

    function result = apply(obj, pixels)
      %APPLY Transform pixel data through the pipeline.
      %   result = cmm.apply([0.5 0.3 0.1])
      %   results = cmm.apply(rand(100, 3))
      %
      %   Input:  N x srcChannels matrix (double or single)
      %   Output: N x dstChannels double matrix
      if ~obj.IsReady
        error('iccdev:notReady', 'Call begin() before apply().');
      end
      result = icc_mex('cmm_apply', obj.Handle, pixels);
    end

    function ah = get_apply(obj)
      %GET_APPLY Create a thread-safe apply handle.
      %   ah = cmm.get_apply();
      if ~obj.IsReady
        error('iccdev:notReady', 'Call begin() before get_apply().');
      end
      ah = iccdev.IccApply(obj, obj.SrcChannels, obj.DstChannels);
    end

    function info = get_info(obj)
      %GET_INFO Get CMM pipeline info struct.
      info = icc_mex('cmm_info', obj.Handle);
    end

    function close(obj)
      %CLOSE Release the CMM and all attached profiles.
      if obj.Handle ~= uint64(0)
        icc_mex('cmm_free', obj.Handle);
        obj.Handle = uint64(0);
        obj.IsReady = false;
      end
    end

    function delete(obj)
      obj.close();
    end

    function disp(obj)
      if ~obj.is_valid()
        fprintf('IccCmm: <closed>\n');
      elseif ~obj.IsReady
        fprintf('IccCmm: <not initialized>\n');
      else
        fprintf('IccCmm: src=%d ch, dst=%d ch\n', ...
          obj.SrcChannels, obj.DstChannels);
      end
    end

    function tf = is_valid(obj)
      tf = obj.Handle ~= uint64(0);
    end
  end

  methods (Static, Hidden)
    function varargout = call_mex_for_test(varargin)
      %CALL_MEX_FOR_TEST Allow tests to exercise private native lifecycle checks.
      if nargout > 0
        [varargout{1:nargout}] = icc_mex(varargin{:});
      else
        icc_mex(varargin{:});
      end
    end
  end
end
