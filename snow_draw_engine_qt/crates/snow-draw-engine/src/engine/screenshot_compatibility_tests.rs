use super::*;
use snow_draw_engine_core::arrow::{ArrowType, StrokeStyle};
use snow_draw_engine_document::{ArrowData, ElementMeta, TextLayoutSize, Transaction};
use snow_draw_engine_editor::{TextCommitTarget, TextDraftCommit};

fn setup() -> (Engine, ViewportId) {
    let mut engine = Engine::default();
    let viewport = engine.create_viewport(ViewportConfig::default()).unwrap();
    engine
        .set_viewport_surface_size(viewport, 800, 600)
        .unwrap();
    (engine, viewport)
}

fn drafts(states: &[&str]) -> Vec<TextDraftCommit> {
    states
        .iter()
        .map(|text| {
            TextDraftCommit::new(
                TextCommitTarget::New,
                Point::new(100.0, 100.0),
                *text,
                TextLayoutSize::new(100.0, 24.0),
                EditorStyleDefaults::default().text,
                true,
                false,
            )
        })
        .collect()
}

#[test]
fn screenshot_text_history_undoes_arrow_then_actual_character_edits_and_redoes() {
    let (mut engine, viewport) = setup();
    let text_id = engine.model.peek_next_element_id();
    engine
        .commit_text_history_with_viewport_changes(
            viewport,
            drafts(&["文", "文字", "文字测", "文字测试"]),
        )
        .unwrap();
    let arrow_id = engine.model.peek_next_element_id();
    let mut transaction = Transaction::new("Create arrow after text");
    transaction.insert_arrow(
        arrow_id,
        ElementMeta::default(),
        ArrowData::from_global_points(
            &[Point::new(10.0, 10.0), Point::new(40.0, 40.0)],
            snow_draw_engine_core::ColorRgba8 {
                r: 255,
                g: 0,
                b: 0,
                a: 255,
            },
            2.0,
            StrokeStyle::Solid,
            ArrowType::Straight,
            None,
            None,
        )
        .unwrap(),
    );
    engine
        .commit_transaction(
            viewport,
            ApplyTransactionCommand {
                transaction,
                history_undo_snapshot: None,
                created_selection_target: None,
            },
        )
        .unwrap();
    engine.undo().unwrap();
    assert!(engine.model.arrow(arrow_id).is_err());
    assert_eq!(engine.model.text(text_id).unwrap().text, "文字测试");
    for expected in ["文字测", "文字", "文"] {
        engine.undo().unwrap();
        assert_eq!(engine.model.text(text_id).unwrap().text, expected);
    }
    engine.undo().unwrap();
    assert!(engine.model.paint_order().is_empty());
    assert!(!engine.history_state().can_undo);
    for expected in ["文", "文字", "文字测", "文字测试"] {
        engine.redo().unwrap();
        assert_eq!(engine.model.text(text_id).unwrap().text, expected);
    }
    engine.redo().unwrap();
    assert!(engine.model.arrow(arrow_id).is_ok());
}

#[test]
fn screenshot_text_history_preserves_replacements_and_empty_intermediate_states() {
    let (mut engine, viewport) = setup();
    engine
        .commit_text_history_with_viewport_changes(viewport, drafts(&["abc", "a中c", "", "new"]))
        .unwrap();
    assert_eq!(
        engine
            .model
            .text(engine.model.paint_order()[0])
            .unwrap()
            .text,
        "new"
    );
    engine.undo().unwrap();
    assert!(engine.model.paint_order().is_empty());
    engine.undo().unwrap();
    let restored = engine.model.paint_order()[0];
    assert_eq!(engine.model.text(restored).unwrap().text, "a中c");
    engine.undo().unwrap();
    assert_eq!(engine.model.text(restored).unwrap().text, "abc");
}

#[test]
fn screenshot_text_history_preflights_all_edits_before_mutating() {
    let (mut engine, viewport) = setup();
    let mut history = drafts(&["valid", "invalid"]);
    history[1].measured_layout = TextLayoutSize::new(f64::NAN, 24.0);
    assert!(
        engine
            .commit_text_history_with_viewport_changes(viewport, history)
            .is_err()
    );
    assert!(engine.model.paint_order().is_empty());
    assert!(!engine.history_state().can_undo);
}

#[test]
fn screenshot_watermark_does_not_create_history_or_consume_annotation_redo() {
    let (mut engine, viewport) = setup();
    let mut watermark = engine.watermark_config().clone();
    watermark.text = "watermark".to_owned();
    engine
        .set_viewport_watermark_config_without_history(viewport, watermark.clone())
        .unwrap();
    assert!(!engine.history_state().can_undo);
    engine
        .commit_text_history_with_viewport_changes(viewport, drafts(&["annotation"]))
        .unwrap();
    engine.undo().unwrap();
    let generation = engine.host_history_generation();
    watermark.text.clear();
    engine
        .set_viewport_watermark_config_without_history(viewport, watermark)
        .unwrap();
    assert!(engine.history_state().can_redo);
    assert_eq!(engine.host_history_generation(), generation);
    engine.redo().unwrap();
    assert_eq!(
        engine
            .model
            .text(engine.model.paint_order()[0])
            .unwrap()
            .text,
        "annotation"
    );
    assert!(engine.watermark_config().text.is_empty());
    engine.undo().unwrap();
    assert!(engine.model.paint_order().is_empty());
    assert!(!engine.history_state().can_undo);
}

#[test]
fn screenshot_compatibility_does_not_change_default_text_or_watermark_history() {
    let (mut engine, viewport) = setup();
    engine
        .commit_text_draft_with_viewport_changes(viewport, drafts(&["whole text"]).remove(0))
        .unwrap();
    engine.undo().unwrap();
    assert!(engine.model.paint_order().is_empty());
    let mut watermark = engine.watermark_config().clone();
    watermark.text = "normal history".to_owned();
    engine
        .set_viewport_watermark_config(viewport, watermark)
        .unwrap();
    assert!(engine.history_state().can_undo);
    engine.undo().unwrap();
    assert!(engine.watermark_config().text.is_empty());
}
