////////////////////////////////////////////////////////////////////////////////
// The contents of this file are subject to the Interbase Public
// License Version 1.0 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy
// of the License at http://www.Inprise.com/IPL.html
//
// Software distributed under the License is distributed on an
// "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
// or implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code was created by Inprise Corporation
// and its predecessors. Portions created by Inprise Corporation are
// Copyright (C) Inprise Corporation.
//
// All Rights Reserved.
// Contributor(s): ______________________________________.

#ifndef _fb__enc_crypt_H_
#define _fb__enc_crypt_H_

#include "firebird.h"

namespace Firebird{
////////////////////////////////////////////////////////////////////////////////
//class FB__ENC_crypt

class FB__ENC_crypt
{
 public: //typedefs ------------------------------------------------------
  typedef char TEXT;

 public:
  static const size_t c_RESULT_SIZE = (1 + 4 + 4 + 11 + 1);

 public:
  static void ENC_crypt(TEXT*       buf,
                        size_t      bufSize,
                        const TEXT* key,
                        const TEXT* setting);
 private:
  typedef long SLONG;

  union  C_block;
  struct C_block2;

  struct tag_ctx;

  class tag_data;

 private:
  static int helper__des_setkey(tag_ctx&       ctx,
                                unsigned char* key);

  static int helper__des_cipher(tag_ctx&       ctx,
                                const C_block* in,
                                C_block*       out,
                                SLONG          salt,
                                int            num_iter);

  static void helper__permute(const unsigned char* cp,
                              C_block*             out,
                              const C_block*       p,
                              int                  chars_in);
};//class FB__ENC_crypt

////////////////////////////////////////////////////////////////////////////////
}/*nms Firebird*/
#endif
