//========= Copyright 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::SetupDefaultConditionals()
{
	//
	// PLATFORM Conditionals
	//
	{
		FindOrCreateConditional( "WIN32", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "WIN64", true, CONDITIONAL_PLATFORM );
		
		// LINUX is the platform but the VPC scripts use $LINUX and $DEDICATED 
		// (which we automatically create later).
		FindOrCreateConditional( "LINUX32", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "LINUX64", true, CONDITIONAL_PLATFORM );

		FindOrCreateConditional( "OSX32", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "OSX64", true, CONDITIONAL_PLATFORM );

		FindOrCreateConditional( "X360", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "PS3", true, CONDITIONAL_PLATFORM );
	}
	
	//
	// CUSTOM conditionals
	//
	{
		// setup default custom conditionals
		FindOrCreateConditional( "PROFILE", true, CONDITIONAL_CUSTOM );		
		FindOrCreateConditional( "RETAIL", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "CALLCAP", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "FASTCAP", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "CERT", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "MEMTEST", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "NOFPO", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "POSIX", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "LV", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "DEMO", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "NO_STEAM", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "DVDEMU", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "QTDEBUG", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "NO_CEG", true, CONDITIONAL_CUSTOM );
		FindOrCreateConditional( "UPLOAD_CEG", true, CONDITIONAL_CUSTOM );
	}
}	

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CVPC::GetTargetPlatformName()
{
	for ( int i = 0; i < m_Conditionals.Count(); i++ )
	{
		conditional_t *pConditional = &m_Conditionals[i];
		if ( pConditional->type == CONDITIONAL_PLATFORM && pConditional->m_bDefined )
		{
			return pConditional->name.String();
		}
	}

	// fatal - should have already been default set
	Assert( 0 );
	VPCError( "Unspecified platform." );

	return NULL;
}

//-----------------------------------------------------------------------------
//	Case Insensitive. Returns true if platform conditional has been marked
//	as defined.
//-----------------------------------------------------------------------------
bool CVPC::IsPlatformDefined( const char *pName )
{
	for ( int i=0; i<m_Conditionals.Count(); i++ )
	{
		if ( m_Conditionals[i].type == CONDITIONAL_PLATFORM && !V_stricmp( pName, m_Conditionals[i].name.String() ) )
		{
			return m_Conditionals[i].m_bDefined;
		}

	}

	return false;
}

//-----------------------------------------------------------------------------
//	Case Insensitive
//-----------------------------------------------------------------------------
conditional_t *CVPC::FindOrCreateConditional( const char *pName, bool bCreate, conditionalType_e type )
{
	for (int i=0; i<m_Conditionals.Count(); i++)
	{
		if ( !V_stricmp( pName, m_Conditionals[i].name.String() ) )
		{
			// found
			return &m_Conditionals[i];
		}
	}

	if ( !bCreate )
	{
		return NULL;
	}

	int index = m_Conditionals.AddToTail();

	char tempName[256];
	V_strncpy( tempName, pName, sizeof( tempName ) );
	
	// primary internal use as lower case, but spewed to user as upper for style consistency
	m_Conditionals[index].name = V_strlower( tempName );
	m_Conditionals[index].upperCaseName = V_strupr( tempName );
	m_Conditionals[index].type = type;

	return &m_Conditionals[index];
}

void CVPC::SetConditional( const char *pString, bool bSet )
{
	VPCStatus( false, "Set Conditional: $%s = %s", pString, ( bSet ? "1" : "0" ) );

	conditional_t *pConditional = FindOrCreateConditional( pString, true, CONDITIONAL_CUSTOM );
	if ( !pConditional )
	{
		VPCError( "Failed to find or create $%s conditional", pString );
	}

	pConditional->m_bDefined = bSet;
}

//-----------------------------------------------------------------------------
//	Returns true if string has a conditional of the specified type
//-----------------------------------------------------------------------------
bool CVPC::ConditionHasDefinedType( const char* pCondition, conditionalType_e type )
{
	char	symbol[MAX_SYSTOKENCHARS];

	for ( int i=0; i<m_Conditionals.Count(); i++ )
	{
		if ( m_Conditionals[i].type != type )
			continue;
	
		sprintf( symbol, "$%s", m_Conditionals[i].name.String() );
		if ( V_stristr( pCondition, symbol ) )
		{
			// a define of expected type occurs in the conditional expression
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//	Callback for expression evaluator.
//-----------------------------------------------------------------------------
bool CVPC::ResolveConditionalSymbol( const char *pSymbol )
{
	int offset = 0;

	if ( !V_stricmp( pSymbol, "$0" ) || !V_stricmp( pSymbol, "0" ) )
	{
		return false;
	}
	else if ( !V_stricmp( pSymbol, "$1" ) || !V_stricmp( pSymbol, "1" ) )
	{
		return true;
	}

	if ( pSymbol[0] == '$' )
	{
		offset = 1;
	}

	conditional_t *pConditional = FindOrCreateConditional( (char*)pSymbol+offset, false, CONDITIONAL_NULL );
	if ( pConditional )
	{
		// game conditionals only resolve true when they are 'defined' and 'active'
		// only one game conditional is expected to be active at a time
		if ( pConditional->type == CONDITIONAL_GAME )
		{
			if ( !pConditional->m_bDefined )
			{
				return false;
			}

			return pConditional->m_bGameConditionActive;
		}

		// all other type of conditions are gated by their 'defined' state
		return pConditional->m_bDefined;
	}

	// unknown conditional, defaults to false
	return false;
}

// Er2: Custom VPC expression parser
bool EvalExpression(char* pStr, bool bParens = false, char **ppOut = nullptr)
{
	bool bNot = false, bVal = false, bVal2 = false;

	while (*pStr)
	{
		switch (*pStr++)
		{
		case '!':
			bNot = !bNot;
			break;

		case '$':
		{
			// Get variable name
			char* str = pStr;
			while (*pStr && (V_isalnum(*pStr) || *pStr == '_'))
				pStr++;
			char cSaved = *pStr;
			if (cSaved) *pStr = '\0'; // for C strings
			bVal = EvaluateConditional(str) || g_pVPC->ResolveConditionalSymbol(str);
			bVal ^= bNot;
			*pStr++ = cSaved; // to allow syntax like [!$WIN64&&!$VS2015]
			goto out;
			break;
		}
		case '0': bVal = false; goto out; break;
		case '1': bVal = true;  goto out; break;

		case '|': // ||
			if (*pStr++ != '|')
				g_pVPC->VPCSyntaxError("invalid 'or', should be '||'");
			bVal2 = EvalExpression(pStr, false, &pStr);
			bVal = bVal || bVal2;
			break;

		case '&': // &&
			if (*pStr++ != '&')
				g_pVPC->VPCSyntaxError("invalid 'and', should be '&&'");
			bVal2 = EvalExpression(pStr, false, &pStr);
			bVal = bVal && bVal2;
			break;

		case '(':
			bVal = EvalExpression(pStr, true, &pStr);
			break;
		case ')':
			if (!bParens)
				g_pVPC->VPCSyntaxError("closing parenthesis without opening");
			goto out;
			break;

		case '[':
			break;
		case ']':
			if (*pStr)
				g_pVPC->VPCSyntaxError("junk after condition");
			break;

		case ' ':
			break;
		default:
			g_pVPC->VPCSyntaxError();
		}
	}
	out:
	if (ppOut) *ppOut = pStr;
	return bVal;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::EvaluateConditionalExpression( const char *pExpression )
{
	char conditionalBuffer[MAX_SYSTOKENCHARS];
	ResolveMacrosInConditional( pExpression, conditionalBuffer, sizeof( conditionalBuffer ) );

	if ( !conditionalBuffer[0] )
	{
		// empty string, same as not having a conditional
		return true;
	}
	Msg("%s\n", conditionalBuffer);
	bool bSuccess = EvalExpression((char *)conditionalBuffer);
	Msg(" = %d\n", bSuccess);
	return bSuccess;
}
