/*
 *   File name: PkgManager.cpp
 *   Summary:	Simple package manager support for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "PkgManager.h"
#include "Logger.h"
#include "Exception.h"

#define LOG_COMMANDS	true
#define LOG_OUTPUT	false
#include "SysUtil.h"


#define CACHE_SIZE		500
#define CACHE_COST		1

#define VERBOSE_PKG_QUERY	1


using namespace QDirStat;

using SysUtil::runCommand;
using SysUtil::tryRunCommand;
using SysUtil::haveCommand;


PkgQuery * PkgQuery::_instance = 0;


PkgQuery * PkgQuery::instance()
{
    if ( ! _instance )
    {
	_instance = new PkgQuery();
	CHECK_PTR( _instance );
    }

    return _instance;
}


PkgQuery::PkgQuery()
{
    _cache.setMaxCost( CACHE_SIZE );

    checkPkgManager( new DpkgPkgManager() );
    checkPkgManager( new RpmPkgManager()  );

    _pkgManagers += _secondaryPkgManagers;
    _secondaryPkgManagers.clear();
}


PkgQuery::~PkgQuery()
{
    qDeleteAll( _pkgManagers );
}


void PkgQuery::checkPkgManager( PkgManager * pkgManager )
{
    CHECK_PTR( pkgManager );

    if ( pkgManager->isPrimaryPkgManager() )
    {
	logInfo() << "Found primary package manager " << pkgManager->name() << endl;
	_pkgManagers << pkgManager;
    }
    else if ( pkgManager->isAvailable() )
    {
	logInfo() << "Found secondary package manager " << pkgManager->name() << endl;
	_secondaryPkgManagers << pkgManager;
    }
    else
    {
	delete pkgManager;
    }
}


QString PkgQuery::owningPkg( const QString & path )
{
    return instance()->getOwningPackage( path );
}


QString PkgQuery::getOwningPackage( const QString & path )
{
    QString pkg = "";
    QString foundBy;
    bool haveResult = false;

    if ( _cache.contains( path ) )
    {
	haveResult = true;
	foundBy	   = "Cache";
	pkg	   = *( _cache[ path ] );
    }


    if ( ! haveResult )
    {
	foreach ( PkgManager * pkgManager, _pkgManagers )
	{
	    pkg = pkgManager->owningPkg( path );

	    if ( ! pkg.isEmpty() )
	    {
		haveResult = true;
		foundBy	   = pkgManager->name();
		break;
	    }
	}

	if ( foundBy.isEmpty() )
	    foundBy = "all";

	// Insert package name (even if empty) into the cache
	_cache.insert( path, new QString( pkg ), CACHE_COST );
    }

#if VERBOSE_PKG_QUERY
    if ( pkg.isEmpty() )
	logDebug() << foundBy << ": No package owns " << path << endl;
    else
	logDebug() << foundBy << ": Package " << pkg << " owns " << path << endl;
#endif

    return pkg;
}




bool DpkgPkgManager::isPrimaryPkgManager()
{
    return tryRunCommand( "/usr/bin/dpkg -S /usr/bin/dpkg", QRegExp( "^dpkg:.*" ) );
}


bool DpkgPkgManager::isAvailable()
{
    return haveCommand( "/usr/bin/dpkg" );
}


QString DpkgPkgManager::owningPkg( const QString & path )
{
    int exitCode = -1;
    QString output = runCommand( "/usr/bin/dpkg", QStringList() << "-S" << path, &exitCode );

    if ( exitCode != 0 || output.contains( "no path found matching pattern" ) )
	return "";

    QString pkg = output.section( ":", 0, 0 );

    return pkg;
}




bool RpmPkgManager::isPrimaryPkgManager()
{
    // Using /bin/rpm, not /usr/bin/rpm because older systems only have
    // /bin/rpm, but they all have at least a symlink /bin/rpm -> /usr/bin/rpm
    // so it's safe to use /bin/rpm for both old and new systems.

    return tryRunCommand( "/bin/rpm -qf /bin/rpm", QRegExp( "^rpm.*" ) );
}


bool RpmPkgManager::isAvailable()
{
    return haveCommand( "/bin/rpm" );
}


QString RpmPkgManager::owningPkg( const QString & path )
{
    int exitCode = -1;
    QString output = runCommand( "/bin/rpm",
				 QStringList() << "-qf" << "--queryformat" << "%{NAME}" << path,
				 &exitCode );

    if ( exitCode != 0 || output.contains( "not owned by any package" ) )
	return "";

    QString pkg = output;

    return pkg;
}
