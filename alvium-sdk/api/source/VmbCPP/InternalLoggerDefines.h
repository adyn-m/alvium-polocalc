/*=============================================================================
  Copyright (C) 2024 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        InternalLoggerDefines.h

  Description:  Definition of internal logging macros

-------------------------------------------------------------------------------

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF TITLE,
  NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR  PURPOSE ARE
  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, 
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  
  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#ifndef VMBCPP_INTERNALLOGGERDEFINES_H
#define VMBCPP_INTERNALLOGGERDEFINES_H

#include <VmbCPP/VmbSystem.h>

/**
* \file        InternalLoggerDefines.h
*
* \brief       Definition of internal logging macros
*/

/**
 * \brief Macro for logging the provided text.
 *
 * The function using the macro is logged too.
 *
 * \note May throw std::bad_alloc, if there is insufficient memory to create
 * log message string.
 */
#define LOG_FREE_TEXT( txt )                                                \
{                                                                           \
    std::string strExc( txt );                                              \
    strExc.append( " in function: " );                                      \
    strExc.append( __FUNCTION__ );                                          \
    LOGGER_LOG(VmbSystem::GetInstance().GetLogger(), std::move(strExc));    \
}

#define LOG_ERROR( txt, errCode )                                           \
{                                                                           \
    std::string strExc( txt );                                              \
    strExc.append( " in function: " );                                      \
    strExc.append( __FUNCTION__ );                                          \
    strExc.append( ", VmbErrorType: ");                                            \
    strExc.append( std::to_string(errCode) );                               \
    LOGGER_LOG(VmbSystem::GetInstance().GetLogger(), std::move(strExc));    \
}

#endif
