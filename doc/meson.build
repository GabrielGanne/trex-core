asciidoc = find_program('asciidoc')
sphinx = find_program('sphinx-build')
doc_builder = files('build_docs.py')
subdir('targets')

hlt_args = custom_target('hlt-arguments',
  input: '../scripts/automation/trex_control_plane/interactive/trex/stl/trex_stl_hltapi.py',
  output: 'hlt_args.asciidoc', command: [python, doc_builder, 'hlt', '@INPUT@', '@OUTPUT@'])
assets = run_command(python, doc_builder, 'assets', meson.current_source_dir(), check: true).stdout().strip().split('\n')
doc_assets = custom_target('documentation-assets', input: files(assets), output: 'assets.stamp',
  command: [python, doc_builder, 'copy-assets', meson.current_source_dir(), meson.current_build_dir(), '@OUTPUT@', '@INPUT@'],
  build_by_default: true)
doc_inputs = files(run_command(python, doc_builder, 'sources', meson.current_source_dir(), check: true).stdout().strip().split('\n'))
foreach document : html_documents
  outputs = [document[1]]
  if document[2].startswith('toc')
    outputs += document[1].replace('.html', '.json')
  endif
  custom_target(document[1], input: [document[0], 'waf.css'], output: outputs,
    command: [python, doc_builder, 'html', asciidoc, '@INPUT0@', '@INPUT1@', '@OUTPUT0@', document[2], hlt_args],
    depend_files: doc_inputs + files('HTMLInjector.py', 'toc.py', 'hlt_args.py'), depends: doc_assets,
    build_by_default: true, install: true, install_dir: runtime_dir / 'doc')
endforeach
if get_option('docs_pdf')
  a2x = find_program('a2x')
  foreach document : pdf_documents
    custom_target(document.replace('.asciidoc', '.pdf'), input: document,
      output: document.replace('.asciidoc', '.pdf'),
      command: [python, doc_builder, 'pdf', a2x, '@INPUT@', '@OUTPUT@', hlt_args],
      depend_files: doc_inputs, build_by_default: true, install: true, install_dir: runtime_dir / 'doc')
  endforeach
endif
foreach api : [['doc', 'cp_docs'], ['doc_stl', 'cp_stl_docs'], ['doc_astf', 'cp_astf_docs']]
  api_source = source_root / 'scripts/automation/trex_control_plane' / api[0]
  custom_target(api[1], output: api[1] + '.stamp',
    command: [python, doc_builder, 'sphinx', sphinx, api_source, meson.current_build_dir() / api[1], '@OUTPUT@'],
    build_always_stale: true, build_by_default: true)
endforeach
meson.add_install_script(python, doc_builder, 'install-assets', meson.current_build_dir(), runtime_dir / 'doc')
