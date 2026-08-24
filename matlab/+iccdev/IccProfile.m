classdef IccProfile < handle
  %ICCPROFILE Read and inspect ICC color profiles.
  %
  %   repo_root = fileparts(fileparts(which('build_mex')));
  %   profile_path = fullfile(repo_root, 'Testing', ...
  %     'sRGB_v4_ICC_preference.icc');
  %   p = iccdev.IccProfile(profile_path)
  %   p = iccdev.IccProfile(profile_path, 'lazy', false)
  %   h = p.header();
  %   disp(h.versionString)
  %   p.close();
  %
  %   % Context-manager pattern:
  %   p = iccdev.IccProfile(profile_path);
  %   c = onCleanup(@() p.close());
  %   disp(p.header())
  %
  % Copyright (c) International Color Consortium.
  % BSD 3-Clause License. See LICENSE.md for details.

  properties (SetAccess = private)
    Handle = uint64(0)
    FilePath = ''
  end

  properties (Access = private)
    CachedHeader = []
  end

  methods
    function obj = IccProfile(filename, varargin)
      %ICCPROFILE Open an ICC profile.
      %   p = iccdev.IccProfile(filename)
      %   p = iccdev.IccProfile(filename, 'lazy', true)
      %   p = iccdev.IccProfile(filename, 'lazy', false)
      lazy = true;
      for k = 1:2:numel(varargin)
        if strcmpi(varargin{k}, 'lazy')
          lazy = logical(varargin{k+1});
        end
      end
      if lazy
        obj.Handle = icc_mex('profile_open', char(filename));
      else
        obj.Handle = icc_mex('profile_read', char(filename));
      end
      obj.FilePath = char(filename);
    end

    function hdr = get_header(obj)
      %GET_HEADER Read the ICC profile header as a struct.
      if isempty(obj.CachedHeader)
        obj.CachedHeader = icc_mex('profile_header', obj.Handle);
      end
      hdr = obj.CachedHeader;
    end

    function hdr = header(obj)
      %HEADER Alias method for get_header; call as p.header().
      hdr = obj.get_header();
    end

    function cs = color_space(obj)
      %COLOR_SPACE Return the color space signature as uint32.
      hdr = obj.get_header();
      cs = uint32(hdr.colorSpace);
    end

    function cs = color_space_name(obj)
      %COLOR_SPACE_NAME Return color space as a readable string.
      cs = iccdev.sig_to_str(obj.color_space());
    end

    function v = version_string(obj)
      %VERSION_STRING Profile version (e.g., '4.3.0').
      hdr = obj.get_header();
      v = hdr.versionString;
    end

    function tf = is_valid(obj)
      %IS_VALID True if the profile handle is open.
      tf = obj.Handle ~= uint64(0);
    end

    function close(obj)
      %CLOSE Release the profile handle.
      if obj.Handle ~= uint64(0)
        icc_mex('profile_free', obj.Handle);
        obj.Handle = uint64(0);
        obj.CachedHeader = [];
      end
    end

    function delete(obj)
      obj.close();
    end

    function disp(obj)
      if obj.is_valid()
        hdr = obj.get_header();
        fprintf('IccProfile: %s\n', obj.FilePath);
        fprintf('  Version:     %s\n', hdr.versionString);
        fprintf('  Color space: %s\n', iccdev.sig_to_str(uint32(hdr.colorSpace)));
        fprintf('  PCS:         %s\n', iccdev.sig_to_str(uint32(hdr.pcs)));
        fprintf('  Size:        %d bytes\n', hdr.size);
      else
        fprintf('IccProfile: <closed>\n');
      end
    end
  end
end
