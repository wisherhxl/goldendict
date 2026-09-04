// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>

#include <memory>

#include "langcoder.hh"

namespace Qt5AcceptanceObserver {
namespace {

int const kIndexingTimeoutMilliseconds = 5 * 60 * 1000;

QString FormatForComponents( std::vector< std::string > const & components )
{
  for( std::vector< std::string >::const_iterator component = components.begin();
       component != components.end(); ++component )
  {
    QString const path = QString::fromUtf8( component->c_str() ).toLower();
    if( path.endsWith( ".dsl" ) || path.endsWith( ".dsl.dz" ) )
      return "dsl";
    if( path.endsWith( ".mdx" ) )
      return "mdict";
    if( path.endsWith( ".ifo" ) )
      return "stardict";
    if( path.endsWith( ".index" ) )
      return "dictd";
    if( path.endsWith( ".dct" ) )
      return "sdict";
    if( path.endsWith( ".xdxf" ) || path.endsWith( ".xdxf.dz" ) )
      return "xdxf";
    if( path.endsWith( ".gls" ) || path.endsWith( ".gls.dz" ) )
      return "gls";
    if( path.endsWith( ".bgl" ) )
      return "bgl";
    if( path.endsWith( ".aar" ) )
      return "aard";
    if( path.endsWith( ".zim" ) || path.endsWith( ".zimaa" ) )
      return "zim";
    if( path.endsWith( ".slob" ) )
      return "slob";
    if( path.endsWith( ".lsa" ) )
      return "lsa";
    if( path.endsWith( ".zip" ) )
      return "zipsounds";
    if( path.endsWith( "/honmon" ) || path.endsWith( "\\honmon" ) )
      return "epwing";
  }
  return QString();
}

QJsonObject CatalogEntry( Dictionary::Class & dictionary, int order )
{
  QJsonArray components;
  std::vector< std::string > const & files = dictionary.getDictionaryFilenames();
  for( std::vector< std::string >::const_iterator file = files.begin();
       file != files.end(); ++file )
    components.append( QString::fromUtf8( file->c_str() ) );

  QJsonObject entry;
  entry.insert( "article_count", static_cast< qint64 >( dictionary.getArticleCount() ) );
  entry.insert( "components", components );
  entry.insert( "edition", QString() );
  entry.insert( "enabled", true );
  entry.insert( "format", FormatForComponents( files ) );
  entry.insert( "headword_count", static_cast< qint64 >( dictionary.getWordCount() ) );
  entry.insert( "id", QString::fromUtf8( dictionary.getId().c_str() ) );
  entry.insert( "name", QString::fromUtf8( dictionary.getName().c_str() ) );
  entry.insert( "order", order );
  entry.insert( "source_language", LangCoder::intToCode2( dictionary.getLangFrom() ) );
  entry.insert( "target_language", LangCoder::intToCode2( dictionary.getLangTo() ) );
  return entry;
}

void ScheduleExit()
{
  QTimer::singleShot( 0, qApp, SLOT( quit() ) );
}

QJsonArray Catalog( std::vector< sptr< Dictionary::Class > > const & dictionaries )
{
  QJsonArray catalog;
  int order = 0;
  for( std::vector< sptr< Dictionary::Class > >::const_iterator dictionary = dictionaries.begin();
       dictionary != dictionaries.end(); ++dictionary )
  {
    if( ( *dictionary )->getDictionaryFilenames().empty() )
      continue;
    catalog.append( CatalogEntry( **dictionary, order++ ) );
  }
  return catalog;
}

QJsonObject Phase( QByteArray const & name, QByteArray const & status,
                   int sequence, std::string const & dictionaryId = std::string() )
{
  QJsonObject phase;
  phase.insert( "dictionary_id", QString::fromUtf8( dictionaryId.c_str() ) );
  phase.insert( "name", QString::fromLatin1( name ) );
  phase.insert( "sequence", sequence );
  phase.insert( "status", QString::fromLatin1( status ) );
  return phase;
}

bool WriteRaw( QByteArray const & outputPath, QByteArray const & conditionsHash,
               QByteArray const & scenario, QJsonArray const & catalog,
               QJsonArray const & phases, QByteArray const & outcome )
{
  QJsonObject raw;
  raw.insert( "catalog", catalog );
  raw.insert( "conditions_sha256", QString::fromLatin1( conditionsHash ) );
  raw.insert( "elapsed_milliseconds", 0 );
  raw.insert( "errors", QJsonArray() );
  raw.insert( "outcome", QString::fromLatin1( outcome ) );
  raw.insert( "phases", phases );
  raw.insert( "scenario", QString::fromLatin1( scenario ) );
  raw.insert( "schema", "goldendict-real-dictionary-raw-observation-v1" );

  QSaveFile output( QString::fromUtf8( outputPath ) );
  if( !output.open( QIODevice::WriteOnly ) )
    return false;
  QByteArray const content = QJsonDocument( raw ).toJson( QJsonDocument::Compact ) + '\n';
  return output.write( content ) == content.size() && output.commit();
}

Dictionary::Class * FindIndexingDictionary(
  std::vector< sptr< Dictionary::Class > > const & dictionaries,
  QString const & name )
{
  for( std::vector< sptr< Dictionary::Class > >::const_iterator dictionary = dictionaries.begin();
       dictionary != dictionaries.end(); ++dictionary )
    if( QString::fromUtf8( ( *dictionary )->getName().c_str() ) == name )
      return dictionary->get();
  return 0;
}

} // unnamed namespace

void Publish( QObject * owner,
              std::vector< sptr< Dictionary::Class > > const & dictionaries,
              FTS::FtsIndexing & ftsIndexing )
{
  QByteArray const outputPath = qgetenv( "GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH" );
  if( outputPath.isEmpty() )
    return;

  QByteArray const conditionsHash = qgetenv( "GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256" );
  QByteArray const scenario = qgetenv( "GOLDENDICT_ACCEPTANCE_SCENARIO" );
  if( conditionsHash.isEmpty()
      || ( scenario != "clean-discovery" && scenario != "warm-restart"
           && scenario != "explicit-rescan" && scenario != "changed-source"
           && scenario != "cancellation" && scenario != "cancellation-recovery"
           && scenario != "unavailable-companion"
           && scenario != "companion-recovery" ) )
  {
    ScheduleExit();
    return;
  }

  static bool rescanRequested = false;
  if( scenario == "explicit-rescan" && !rescanRequested )
  {
    rescanRequested = true;
    if( !QMetaObject::invokeMethod( owner, "on_rescanFiles_triggered",
                                   Qt::QueuedConnection ) )
      ScheduleExit();
    return;
  }

  if( scenario == "cancellation" || scenario == "cancellation-recovery" )
  {
    QTimer * timer = new QTimer( owner );
    std::shared_ptr< QElapsedTimer > elapsed( new QElapsedTimer );
    std::shared_ptr< bool > started( new bool( false ) );
    std::shared_ptr< std::string > dictionaryId( new std::string );
    elapsed->start();
    QObject::connect( timer, &QTimer::timeout, owner,
                      [=, &dictionaries, &ftsIndexing]() {
      QString const indexingName = ftsIndexing.nowIndexingName();
      if( !*started && !indexingName.isEmpty() )
      {
        Dictionary::Class * dictionary = FindIndexingDictionary( dictionaries, indexingName );
        if( !dictionary
            || ( scenario == "cancellation" && dictionary->haveFTSIndex() ) )
        {
          timer->stop();
          qApp->exit( 4 );
          return;
        }
        *started = true;
        *dictionaryId = dictionary->getId();
        if( scenario == "cancellation" )
        {
          ftsIndexing.stopIndexing();
          if( dictionary->haveFTSIndex() )
          {
            timer->stop();
            qApp->exit( 4 );
            return;
          }
          QJsonArray phases;
          phases.append( Phase( "full-text-indexing", "started", 0, *dictionaryId ) );
          phases.append( Phase( "full-text-indexing", "cancelled", 1, *dictionaryId ) );
          timer->stop();
          if( !WriteRaw( outputPath, conditionsHash, scenario, Catalog( dictionaries ),
                         phases, "cancelled" ) )
          {
            qApp->exit( 4 );
            return;
          }
          ScheduleExit();
        }
        return;
      }
      if( scenario == "cancellation-recovery" && *started && indexingName.isEmpty() )
      {
        Dictionary::Class * dictionary = 0;
        for( std::vector< sptr< Dictionary::Class > >::const_iterator item = dictionaries.begin();
             item != dictionaries.end(); ++item )
          if( ( *item )->getId() == *dictionaryId )
            dictionary = item->get();
        if( !dictionary || !dictionary->haveFTSIndex() )
        {
          timer->stop();
          qApp->exit( 4 );
          return;
        }
        QJsonArray phases;
        phases.append( Phase( "full-text-indexing", "started", 0, *dictionaryId ) );
        phases.append( Phase( "full-text-indexing", "completed", 1, *dictionaryId ) );
        timer->stop();
        if( !WriteRaw( outputPath, conditionsHash, scenario, Catalog( dictionaries ),
                       phases, "completed" ) )
        {
          qApp->exit( 4 );
          return;
        }
        ScheduleExit();
        return;
      }
      if( elapsed->elapsed() > kIndexingTimeoutMilliseconds )
      {
        timer->stop();
        qApp->exit( 4 );
      }
    } );
    timer->start( 1 );
    return;
  }

  QJsonArray const catalog = Catalog( dictionaries );

  QJsonArray phases;
  QJsonObject started;
  started.insert( "dictionary_id", QString() );
  QByteArray const phaseName = scenario == "warm-restart" ? "restart" :
                               scenario == "explicit-rescan" ? "rescan" :
                               scenario == "changed-source" ? "source-change" :
                               scenario == "unavailable-companion" ? "companion-unavailable" :
                               scenario == "companion-recovery" ? "companion-recovery" :
                               "discovery";
  started.insert( "name", QString::fromLatin1( phaseName ) );
  started.insert( "sequence", 0 );
  started.insert( "status", "started" );
  phases.append( started );
  QJsonObject completed;
  completed.insert( "dictionary_id", QString() );
  completed.insert( "name", QString::fromLatin1( phaseName ) );
  completed.insert( "sequence", 1 );
  completed.insert( "status", "completed" );
  phases.append( completed );

  WriteRaw( outputPath, conditionsHash, scenario, catalog, phases, "completed" );
  ScheduleExit();
}

} // namespace Qt5AcceptanceObserver
