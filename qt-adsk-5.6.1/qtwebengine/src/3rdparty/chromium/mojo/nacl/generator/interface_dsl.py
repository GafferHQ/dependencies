# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class Interface(object):
  def __init__(self):
    self.functions = []

  def Func(self, name, return_type):
    f = Function(self, len(self.functions), name, return_type)
    self.functions.append(f)
    return f

  def Finalize(self):
    for f in self.functions:
      f.Finalize()

class Function(object):
  def __init__(self, parent, uid, name, return_type):
    self.parent = parent
    self.uid = uid
    self.name = name
    self.return_type = return_type
    self.params = []
    self.param_by_name = {}
    self.result_param = None

  def Param(self, name, param_type=None):
    p = Param(self, len(self.params), name, param_type)
    self.params.append(p)
    self.param_by_name[name] = p
    return p

  def ParamList(self):
    return [param.param_type + ' ' + param.name for param in self.params]

  def ParamDecl(self):
    if self.params:
      return ', '.join(self.ParamList())
    else:
      return 'void'

  def Finalize(self):
    self.result_param = Param(self, len(self.params), 'result')
    self.result_param.Out(self.return_type).AlwaysWritten()

class Param(object):
  def __init__(self, parent, uid, name, param_type=None):
    self.parent = parent
    self.uid = uid
    self.name = name
    self.base_type = param_type
    self.param_type = param_type
    self.size = None
    self.is_input = False
    self.is_output = False
    self.is_array = False
    self.is_struct = False
    self.is_extensible = False
    self.is_optional = False
    self.is_always_written = False

  def GetSizeParam(self):
    assert self.size
    return self.parent.param_by_name[self.size]

  def In(self, ty):
    self.base_type = ty
    self.param_type = ty
    self.is_input = True
    return self

  def InArray(self, ty, size):
    self.base_type = ty
    self.param_type = 'const ' + ty + '*'
    self.size = size
    self.is_input = True
    self.is_array = True
    return self

  # An "extensible" struct is one where we don't know the exact size - rather
  # the first 4 bytes of the struct declare the length of the struct.  This
  # allows forwards and backwards compatibility with additive changes to the
  # structure definition.
  def InExtensibleStruct(self, ty):
    self.base_type = ty
    self.param_type = 'const struct ' + ty + '*'
    self.is_input = True
    self.is_struct = True
    self.is_extensible = True
    return self

  def InOut(self, ty):
    self.base_type = ty
    self.param_type = ty + '*'
    self.is_input = True
    self.is_output = True
    return self

  def Out(self, ty):
    self.base_type = ty
    self.param_type = ty + '*'
    self.is_output = True
    return self

  def OutArray(self, ty, size):
    self.base_type = ty
    self.param_type = ty + '*'
    self.size = size
    self.is_array = True
    self.is_output = True
    return self

  # The size of the struct is fixed by the API, it cannot be extended.
  def OutFixedStruct(self, ty):
    self.base_type = ty
    self.param_type = 'struct ' + ty + '*'
    self.is_output = True
    self.is_struct = True
    self.is_extensible = False
    return self

  def OutFixedStructArray(self, ty, size):
    self.base_type = ty
    self.param_type = 'struct ' + ty + '*'
    self.size = size
    self.is_array = True
    self.is_output = True
    return self

  # Declares that it is valid to pass a null pointer.
  def Optional(self):
    assert not self.IsPassedByValue()
    self.is_optional = True
    return self

  def AlwaysWritten(self):
    assert self.is_output, self
    self.is_always_written = True
    return self

  def IsScalar(self):
    return not self.is_array and not self.is_struct

  def IsPassedByValue(self):
    return not self.is_output and self.IsScalar()
