//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"

#include "vcollide_parse_private.h"

#include "vphysics/constraints.h"
#include "vphysics/vehicles.h"

#include "filesystem_helpers.h"
#include "bspfile.h"

#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void ReadVector( const char *pString, Vector& out )
{
	float x = 0, y = 0, z = 0;
	const int read = sscanf( pString, "%f %f %f", &x, &y, &z );
	if (read == 3)
	{
		out[0] = x;
		out[1] = y;
		out[2] = z;
	}
	else
	{
		AssertMsg( false, "Unable to read 3D vector components from '%s'.", pString );
		Warning( "Unable to read 3D vector components from '%s'.", pString );
	}
}

// dimhotepus: Unused.
//static void ReadAngles( const char *pString, QAngle& out )
//{
//	float x = 0, y = 0, z = 0;
//	sscanf( pString, "%f %f %f", &x, &y, &z );
//	out[0] = x;
//	out[1] = y;
//	out[2] = z;
//}

static void ReadVector4D( const char *pString, Vector4D& out )
{
	float x = 0, y = 0, z = 0, w = 0;
	const int read = sscanf( pString, "%f %f %f %f", &x, &y, &z, &w );
	if (read == 4)
	{
		out[0] = x;
		out[1] = y;
		out[2] = z;
		out[3] = w;
	}
	else
	{
		AssertMsg( false, "Unable to read 4D vector components from '%s'.", pString );
		Warning( "Unable to read 4D vector components from '%s'.", pString );
	}
}

class CVPhysicsParse final : public IVPhysicsKeyParser
{
public:
				CVPhysicsParse( const char *pKeyData );
				~CVPhysicsParse() = default;
	void		NextBlock( void );

	const char *GetCurrentBlockName( void ) override;
	bool		Finished( void ) override;
	void		ParseSolid( solid_t *pSolid, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseFluid( fluid_t *pFluid, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseRagdollConstraint( constraint_ragdollparams_t *pConstraint, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseSurfaceTable( intp *table, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseSurfaceTablePacked( CUtlVector<char> &out );
	void		ParseVehicle( vehicleparams_t *pVehicle, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseCustom( void *pCustom, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		SkipBlock( void ) override { ParseCustom(NULL, NULL); }

private:
	void		ParseVehicleAxle( vehicle_axleparams_t &axle );
	void		ParseVehicleWheel( vehicle_wheelparams_t &wheel );
	void		ParseVehicleSuspension( vehicle_suspensionparams_t &suspension );
	void		ParseVehicleBody( vehicle_bodyparams_t &body );
	void		ParseVehicleEngine( vehicle_engineparams_t &engine );
	void		ParseVehicleEngineBoost( vehicle_engineparams_t &engine );
	void		ParseVehicleSteering( vehicle_steeringparams_t &steering );

	const char *m_pText;
	char m_blockName[MAX_KEYVALUE];
};


CVPhysicsParse::CVPhysicsParse( const char *pKeyData )
{
	m_pText = pKeyData;
	m_blockName[0] = '\0';
	NextBlock();
}

void CVPhysicsParse::NextBlock( void )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( V_streq(value, "{") )
		{
			V_strcpy_safe( m_blockName, key );
			return;
		}
	}

	// Make sure m_blockName is initialized -- should this be done?
	m_blockName[ 0 ] = 0;
}


const char *CVPhysicsParse::GetCurrentBlockName( void )
{
	if ( m_pText )
		return m_blockName;

	return NULL;
}


bool CVPhysicsParse::Finished( void )
{
	if ( m_pText )
		return false;

	return true;
}


void CVPhysicsParse::ParseSolid( solid_t *pSolid, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];
	key[0] = 0;
	value[0] = 0;

	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pSolid );
	}
	else
	{
		memset( pSolid, 0, sizeof(*pSolid) );
	}
	
	// disable these until the ragdoll is created
	pSolid->params.enableCollisions = false;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		if ( V_strieq( key, "index" ) )
		{
			pSolid->index = atoi(value);
		}
		else if ( V_strieq( key, "name" ) )
		{
			Q_strncpy( pSolid->name, value, sizeof(pSolid->name) );
		}
		else if ( V_strieq( key, "parent" ) )
		{
			Q_strncpy( pSolid->parent, value, sizeof(pSolid->parent) );
		}
		else if ( V_strieq( key, "surfaceprop" ) )
		{
			Q_strncpy( pSolid->surfaceprop, value, sizeof(pSolid->surfaceprop) );
		}
		else if ( V_strieq( key, "mass" ) )
		{
			pSolid->params.mass = strtof(value, nullptr);
		}
		else if ( V_strieq( key, "massCenterOverride" ) )
		{
			ReadVector( value, pSolid->massCenterOverride );
			pSolid->params.massCenterOverride = &pSolid->massCenterOverride;
		}
		else if ( V_strieq( key, "inertia" ) )
		{
			pSolid->params.inertia = strtof(value, nullptr);
		}
		else if ( V_strieq( key, "damping" ) )
		{
			pSolid->params.damping = strtof(value, nullptr);
		}
		else if ( V_strieq( key, "rotdamping" ) )
		{
			pSolid->params.rotdamping = strtof(value, nullptr);
		}
		else if ( V_strieq( key, "volume" ) )
		{
			pSolid->params.volume = strtof(value, nullptr);
		}
		else if ( V_strieq( key, "drag" ) )
		{
			pSolid->params.dragCoefficient = strtof(value, nullptr);
		}
		else if ( V_strieq( key, "rollingdrag" ) )
		{
			AssertMsg( false, "Solid '%s' rolling drag is not implemented.", pSolid->name );
			//pSolid->params.rollingDrag = strtof(value, nullptr);
		}
		else
		{
			if ( unknownKeyHandler )
			{
				unknownKeyHandler->ParseKeyValue( pSolid, key, value );
			}
			else
			{
				// dimhotepus: Need to look through data.
				DWarning( "vphysics", 0, "Unknown solid '%s' configuration key '%s' (%s).\n", pSolid->name, key, value );
			}
		}
	}
}

void CVPhysicsParse::ParseRagdollConstraint( constraint_ragdollparams_t *pConstraint, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pConstraint );
	}
	else
	{
		memset( pConstraint, 0, sizeof(*pConstraint) );
		pConstraint->childIndex = -1;
		pConstraint->parentIndex = -1;
	}

	// BUGBUG: xmin/xmax, ymin/ymax, zmin/zmax specify clockwise rotations.  
	// BUGBUG: HL rotations are counter-clockwise, so reverse these limits at some point!!!
	pConstraint->useClockwiseRotations = true;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		if ( V_strieq( key, "parent" ) )
		{
			pConstraint->parentIndex = atoi( value );
		}
		else if ( V_strieq( key, "child" ) )
		{
			pConstraint->childIndex = atoi( value );
		}
		else if ( V_strieq( key, "xmin" ) )
		{
			pConstraint->axes[0].minRotation = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "xmax" ) )
		{
			pConstraint->axes[0].maxRotation = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "xfriction" ) )
		{
			pConstraint->axes[0].angularVelocity = 0;
			pConstraint->axes[0].torque = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "ymin" ) )
		{
			pConstraint->axes[1].minRotation = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "ymax" ) )
		{
			pConstraint->axes[1].maxRotation = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "yfriction" ) )
		{
			pConstraint->axes[1].angularVelocity = 0;
			pConstraint->axes[1].torque = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "zmin" ) )
		{
			pConstraint->axes[2].minRotation = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "zmax" ) )
		{
			pConstraint->axes[2].maxRotation = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "zfriction" ) )
		{
			pConstraint->axes[2].angularVelocity = 0;
			pConstraint->axes[2].torque = strtof( value, nullptr );
		}
		else
		{
			if ( unknownKeyHandler )
			{
				unknownKeyHandler->ParseKeyValue( pConstraint, key, value );
			}
			else
			{
				// dimhotepus: Need to look through data.
				DWarning( "vphysics", 0, "Unknown ragdoll constraint configuration key '%s' (%s).\n", key, value );
			}
		}
	}
}


void CVPhysicsParse::ParseFluid( fluid_t *pFluid, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	pFluid->index = -1;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pFluid );
	}
	else
	{
		memset( pFluid, 0, sizeof(*pFluid) );
		// HACKHACK: This is a reasonable default even though it is hardcoded
		V_strcpy_safe( pFluid->surfaceprop, "water" );
	}

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		if ( V_strieq( key, "index" ) )
		{
			pFluid->index = atoi( value );
		}
		else if ( V_strieq( key, "damping" ) )
		{
			pFluid->params.damping = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "surfaceplane" ) )
		{
			ReadVector4D( value, pFluid->params.surfacePlane );
		}
		else if ( V_strieq( key, "currentvelocity" ) )
		{
			ReadVector( value, pFluid->params.currentVelocity );
		}
		else if ( V_strieq( key, "contents" ) )
		{
			pFluid->params.contents = atoi( value );
		}
		else if ( V_strieq( key, "surfaceprop" ) )
		{
			V_strcpy_safe( pFluid->surfaceprop, value );
		}
		else
		{
			if ( unknownKeyHandler )
			{
				unknownKeyHandler->ParseKeyValue( pFluid, key, value );
			}
			else
			{
				// dimhotepus: Need to look through data.
				DWarning( "vphysics", 0, "Unknown fluid configuration key '%s' (%s).\n", key, value );
			}
		}
	}
}

void CVPhysicsParse::ParseSurfaceTable( intp *table, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		intp propIndex = physprops->GetSurfaceIndex( key );
		int tableIndex = atoi(value);
		if ( tableIndex >= 0 && tableIndex < 128 )
		{
			table[tableIndex] = propIndex;
		}
	}
}

void CVPhysicsParse::ParseSurfaceTablePacked( CUtlVector<char> &out )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	int lastIndex = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		intp len = Q_strlen( key );
		intp outIndex = out.AddMultipleToTail( len + 1 );
		memcpy( &out[outIndex], key, len+1 );
		int tableIndex = atoi(value);
		Assert( tableIndex == lastIndex + 1);
		lastIndex = tableIndex;
	}
}

void CVPhysicsParse::ParseVehicleAxle( vehicle_axleparams_t &axle )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	BitwiseClear( axle );

	key[0] = 0;
	value[0] = 0;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;

		// parse subchunks
		if ( value[0] == '{' )
		{
			if ( V_strieq( key, "wheel" ) )
			{
				ParseVehicleWheel( axle.wheels );
			}
			else if ( V_strieq( key, "suspension" ) )
			{
				ParseVehicleSuspension( axle.suspension );
			}
			else
			{
				SkipBlock();
			}
		}
		else if ( V_strieq( key, "offset" ) )
		{
			ReadVector( value, axle.offset );
		}
		else if ( V_strieq( key, "wheeloffset" ) )
		{
			ReadVector( value, axle.wheelOffset );
		}
		else if ( V_strieq( key, "torquefactor" ) )
		{
			axle.torqueFactor = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "brakefactor" ) )
		{
			axle.brakeFactor = strtof( value, nullptr );
		}
		else
		{
			// dimhotepus: Need to look through data.
			DWarning( "vphysics", 0, "Unknown vehicle axie configuration key '%s' (%s).\n", key, value );
		}
	}
}

void CVPhysicsParse::ParseVehicleWheel( vehicle_wheelparams_t &wheel )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		
		if ( V_strieq( key, "radius" ) )
		{
			wheel.radius = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "mass" ) )
		{
			wheel.mass = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "inertia" ) )
		{
			wheel.inertia = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "damping" ) )
		{
			wheel.damping = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "rotdamping" ) )
		{
			wheel.rotdamping = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "frictionscale" ) )
		{
			wheel.frictionScale = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "material" ) )
		{
			wheel.materialIndex = physprops->GetSurfaceIndex( value );
		}
		else if ( V_strieq( key, "skidmaterial" ) )
		{
			wheel.skidMaterialIndex = physprops->GetSurfaceIndex( value );
		}
		else if ( V_strieq( key, "brakematerial" ) )
		{
			wheel.brakeMaterialIndex = physprops->GetSurfaceIndex( value );
		}
		else
		{
			DWarning( "vphysics", 0, "Unknown vehicle wheel parser configuration key '%s' (%s).\n", key, value );
		}
	}
}


void CVPhysicsParse::ParseVehicleSuspension( vehicle_suspensionparams_t &suspension )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		
		if ( V_strieq( key, "springconstant" ) )
		{
			suspension.springConstant = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "springdamping" ) )
		{
			suspension.springDamping = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "stabilizerconstant" ) )
		{
			suspension.stabilizerConstant = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "springdampingcompression" ) )
		{
			suspension.springDampingCompression = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "maxbodyforce" ) )
		{
			suspension.maxBodyForce = strtof( value, nullptr );
		}
		else
		{
			DWarning( "vphysics", 0, "Unknown vehicle suspension parser configuration key '%s' (%s).\n", key, value);
		}
	}
}


void CVPhysicsParse::ParseVehicleBody( vehicle_bodyparams_t &body )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		
		if ( V_strieq( key, "massCenterOverride" ) )
		{
			ReadVector( value, body.massCenterOverride );
		}
		else if ( V_strieq( key, "addgravity" ) )
		{
			body.addGravity = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "maxAngularVelocity" ) )
		{
			body.maxAngularVelocity = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "massOverride" ) )
		{
			body.massOverride = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "tiltforce" ) )
		{
			body.tiltForce = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "tiltforceheight" ) )
		{
			body.tiltForceHeight = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "countertorquefactor" ) )
		{
			body.counterTorqueFactor = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "keepuprighttorque" ) )
		{
			body.keepUprightTorque = strtof( value, nullptr );
		}
		else
		{
			DWarning( "vphysics", 0, "Unknown vehicle body parser configuration key '%s' (%s).\n", key, value );
		}
	}
}


void CVPhysicsParse::ParseVehicleEngineBoost( vehicle_engineparams_t &engine )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;

		// parse subchunks
		if ( V_strieq( key, "force" ) )
		{
			engine.boostForce = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "duration" ) )
		{
			engine.boostDuration = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "delay" ) )
		{
			engine.boostDelay = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "maxspeed" ) )
		{
			engine.boostMaxSpeed = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "torqueboost" ) )
		{
			engine.torqueBoost = atoi( value ) != 0 ? true : false;
		}
		else
		{
			DWarning( "vphysics", 0, "Unknown vehicle engine boost parser configuration key '%s' (%s).\n", key, value );
		}
	}
}

void CVPhysicsParse::ParseVehicleEngine( vehicle_engineparams_t &engine )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;

		// parse subchunks
		if ( value[0] == '{' )
		{
			if ( V_strieq( key, "boost" ) )
			{
				ParseVehicleEngineBoost( engine );
			}
			else
			{
				SkipBlock();
			}
		}
		else if ( V_strieq( key, "gear" ) )
		{
			// Protect against exploits/overruns
			if ( engine.gearCount < ssize(engine.gearRatio) )
			{
				engine.gearRatio[engine.gearCount] = strtof( value, nullptr );
				engine.gearCount++;
			}
			else
			{
				DWarning( "vphysics", 0, "Out of range (%d > max %zd) gear in vehicle configuration parser '%s' (%s).\n",
					engine.gearCount + 1, ssize(engine.gearRatio), key, value );
				AssertMsg( 0, "Out of range (%d > max %zd) gear in vehicle configuration parser '%s' (%s).\n",
					engine.gearCount + 1, ssize(engine.gearRatio), key, value );
			}
		}
		else if ( V_strieq( key, "horsepower" ) )
		{
			engine.horsepower = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "maxSpeed" ) )
		{
			engine.maxSpeed = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "maxReverseSpeed" ) )
		{
			engine.maxRevSpeed = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "axleratio" ) )
		{
			engine.axleRatio = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "maxRPM" ) )
		{
			engine.maxRPM = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "throttleTime" ) )
		{
			engine.throttleTime = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "AutoTransmission" ) )
		{
			engine.isAutoTransmission = atoi( value ) != 0 ? true : false;
		}
		else if ( V_strieq( key, "shiftUpRPM" ) )
		{
			engine.shiftUpRPM = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "shiftDownRPM" ) )
		{
			engine.shiftDownRPM = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "autobrakeSpeedGain" ) )
		{
			engine.autobrakeSpeedGain = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "autobrakeSpeedFactor" ) )
		{
			engine.autobrakeSpeedFactor = strtof( value, nullptr );
		}
		else
		{
			DWarning( "vphysics", 0, "Unknown vehicle engine parser configuration key '%s' (%s).\n", key, value );
		}
	}
}


void CVPhysicsParse::ParseVehicleSteering( vehicle_steeringparams_t &steering )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;

		// parse subchunks
		if ( V_strieq( key, "degreesSlow" ) )
		{
			steering.degreesSlow = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "degreesFast" ) )
		{
			steering.degreesFast = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "degreesBoost" ) )
		{
			steering.degreesBoost = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "fastcarspeed" ) )
		{
			steering.speedFast = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "slowcarspeed" ) )
		{
			steering.speedSlow = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "slowsteeringrate" ) )
		{
			steering.steeringRateSlow = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "faststeeringrate" ) )
		{
			steering.steeringRateFast = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "steeringRestRateSlow" ) )
		{
			steering.steeringRestRateSlow = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "steeringRestRateFast" ) )
		{
			steering.steeringRestRateFast = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "throttleSteeringRestRateFactor" ) )
		{
			steering.throttleSteeringRestRateFactor = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "boostSteeringRestRateFactor" ) )
		{
			steering.boostSteeringRestRateFactor = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "boostSteeringRateFactor" ) )
		{
			steering.boostSteeringRateFactor = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "steeringExponent" ) )
		{
			steering.steeringExponent = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "turnThrottleReduceSlow" ) )
		{
			steering.turnThrottleReduceSlow = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "turnThrottleReduceFast" ) )
		{
			steering.turnThrottleReduceFast = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "brakeSteeringRateFactor" ) )
		{
			steering.brakeSteeringRateFactor = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "powerSlideAccel" ) )
		{
			steering.powerSlideAccel = strtof( value, nullptr );
		}
		else if ( V_strieq( key, "skidallowed" ) )
		{
			steering.isSkidAllowed = atoi( value ) != 0 ? true : false;
		}
		else if ( V_strieq( key, "dustcloud" ) )
		{
			steering.dustCloud = atoi( value ) != 0 ? true : false;
		}
		else
		{
			DWarning( "vphysics", 0, "Unknown vehicle steering parser configuration key '%s' (%s).\n", key, value );
		}
	}
}

void CVPhysicsParse::ParseVehicle( vehicleparams_t *pVehicle, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pVehicle );
	}
	else
	{
		BitwiseClear( *pVehicle );
	}

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		// parse subchunks
		if ( value[0] == '{' )
		{
			if ( V_strieq( key, "axle" ) )
			{
				// Protect against exploits/overruns
				if ( pVehicle->axleCount < ssize(pVehicle->axles) )
				{
					ParseVehicleAxle( pVehicle->axles[pVehicle->axleCount] );
					pVehicle->axleCount++;
				}
				else
				{
					Assert( 0 );
				}
			}
			else if ( V_strieq( key, "body" ) )
			{
				ParseVehicleBody( pVehicle->body );
			}
			else if ( V_strieq( key, "engine" ) )
			{
				ParseVehicleEngine( pVehicle->engine );
			}
			else if ( V_strieq( key, "steering" ) )
			{
				ParseVehicleSteering( pVehicle->steering );
			}
			else
			{
				SkipBlock();
			}
		}
		else if ( V_strieq( key, "wheelsperaxle" ) )
		{
			pVehicle->wheelsPerAxle = atoi( value );
		}
		else
		{
			DWarning( "vphysics", 0, "Unknown vehicle parser configuration key '%s' (%s).\n", key, value );
		}
	}
}

void CVPhysicsParse::ParseCustom( void *pCustom, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	value[0] = 0;

	int indent = 0;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pCustom );
	}

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );

		if ( m_pText )
		{
			if ( key[0] == '{' )
			{
				indent++;
			}
			else if ( value[0] == '{' )
			{
				// They've got a named block here
				// Increase our indent, and let them parse the key
				indent++;
				if ( unknownKeyHandler )
				{
					unknownKeyHandler->ParseKeyValue( pCustom, key, value );
				}
			}
			else if ( key[0] == '}' )
			{
				indent--;
				if ( indent < 0 )
				{
					NextBlock();
					return;
				}
			}
			else
			{
				if ( unknownKeyHandler )
				{
					unknownKeyHandler->ParseKeyValue( pCustom, key, value );
				}
				// dimhotepus: Do not log missed things here as it is expected.
			}
		}
	}
}

IVPhysicsKeyParser *CreateVPhysicsKeyParser( const char *pKeyData )
{
	return new CVPhysicsParse( pKeyData );
}

void DestroyVPhysicsKeyParser( IVPhysicsKeyParser *pParser )
{
	delete pParser;
}

//-----------------------------------------------------------------------------
// Helper functions for parsing script file
//-----------------------------------------------------------------------------

const char *ParseKeyvalue( const char *pBuffer, OUT_Z_ARRAY char (&key)[MAX_KEYVALUE], OUT_Z_ARRAY char (&value)[MAX_KEYVALUE] )
{
	// Make sure value is always null-terminated.
	value[0] = 0;

	pBuffer = ParseFile( pBuffer, key, NULL );

	// no value on a close brace
	if ( key[0] == '}' && key[1] == 0 )
	{
		value[0] = 0;
		return pBuffer;
	}

	Q_strlower( key );
	
	pBuffer = ParseFile( pBuffer, value, NULL );
	Q_strlower( value );

	return pBuffer;
}
